/******************************************************************************
* Copyright (c) 2016, Connor Manning (connor@hobu.co)
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#include <entwine/tree/registry.hpp>

#include <algorithm>

#include <pdal/PointView.hpp>

#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/tree/chunk.hpp>
#include <entwine/tree/climber.hpp>
#include <entwine/tree/clipper.hpp>
#include <entwine/tree/cold.hpp>
#include <entwine/tree/point-info.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/types/subset.hpp>
#include <entwine/util/storage.hpp>

namespace entwine
{

namespace
{
    bool better(
            const Point& candidate,
            const Point& current,
            const Point& goal,
            const bool is3d)
    {
        if (is3d)   return candidate.sqDist3d(goal) < current.sqDist3d(goal);
        else        return candidate.sqDist2d(goal) < current.sqDist2d(goal);
    }
}

Registry::Registry(
        arbiter::Endpoint& endpoint,
        const Builder& builder,
        const std::size_t clipPoolSize)
    : m_endpoint(endpoint)
    , m_builder(builder)
    , m_structure(builder.structure())
    , m_discardDuplicates(m_structure.discardDuplicates())
    , m_as3d(m_structure.is3d() || m_structure.tubular())
    , m_base()
    , m_cold()
{
    if (m_structure.baseIndexSpan())
    {
        m_base.reset(
                static_cast<BaseChunk*>(
                    Chunk::create(
                        m_builder,
                        m_builder.bbox(),
                        0,
                        m_structure.baseIndexBegin(),
                        m_structure.baseIndexSpan(),
                        true).release()));
    }

    if (m_structure.hasCold())
    {
        m_cold.reset(new Cold(endpoint, m_builder, clipPoolSize));
    }
}

Registry::Registry(
        arbiter::Endpoint& endpoint,
        const Builder& builder,
        const std::size_t clipPoolSize,
        const Json::Value& ids)
    : m_endpoint(endpoint)
    , m_builder(builder)
    , m_structure(builder.structure())
    , m_discardDuplicates(m_structure.discardDuplicates())
    , m_as3d(m_structure.is3d() || m_structure.tubular())
    , m_base()
    , m_cold()
{
    if (m_structure.baseIndexSpan())
    {
        const std::string basePath(
                m_structure.baseIndexBegin().str() +
                m_builder.postfix());

        std::unique_ptr<std::vector<char>> data(
                m_endpoint.tryGetSubpathBinary(basePath));

        if (data)
        {
            m_base.reset(
                    static_cast<BaseChunk*>(
                        Chunk::create(
                            m_builder,
                            m_builder.bbox(),
                            0,
                            m_structure.baseIndexBegin(),
                            m_structure.baseIndexSpan(),
                            std::move(data)).release()));
        }
        else
        {
            std::cout << "No base data found" << std::endl;
            m_base.reset(
                static_cast<BaseChunk*>(
                    Chunk::create(
                        m_builder,
                        m_builder.bbox(),
                        0,
                        m_structure.baseIndexBegin(),
                        m_structure.baseIndexSpan(),
                        true).release()));
        }
    }

    if (m_structure.hasCold())
    {
        m_cold.reset(new Cold(endpoint, builder, clipPoolSize, ids));
    }
}

Registry::~Registry()
{ }

bool Registry::addPoint(
        PooledInfoNode& toAdd,
        Climber& climber,
        Clipper& clipper,
        const std::size_t maxDepth)
{
    bool done(false);

    while (!done)
    {
        if (Cell* cell = getCell(climber, clipper))
        {
            bool redo(false);

            do
            {
                done = false;
                redo = false;

                const PointInfoAtom& atom(cell->atom());
                if (RawInfoNode* current = atom.load())
                {
                    const Point& mid(climber.bbox().mid());
                    const Point& toAddPoint(toAdd->val().point());
                    const Point& currentPoint(current->val().point());

                    if (m_discardDuplicates && toAddPoint == currentPoint)
                    {
                        return false;
                    }

                    if (better(toAddPoint, currentPoint, mid, m_as3d))
                    {
                        done = false;
                        redo = !cell->swap(toAdd, current);
                        if (!redo) toAdd.reset(current);
                    }
                }
                else
                {
                    done = cell->swap(toAdd);
                    redo = !done;
                }
            }
            while (redo);
        }

        if (done)
        {
            climber.count();
            return true;
        }
        else if (
                m_structure.inRange(climber.depth() + 1) &&
                (!maxDepth || climber.depth() + 1 < maxDepth))
        {
            climber.magnify(toAdd->val().point());
        }
        else
        {
            return false;
        }
    }

    return false;
}

Cell* Registry::getCell(const Climber& climber, Clipper& clipper)
{
    Cell* cell(nullptr);

    if (m_structure.isWithinBase(climber.depth()))
    {
        cell = &m_base->getCell(climber);
    }
    else if (m_structure.isWithinCold(climber.depth()))
    {
        cell = &m_cold->getCell(climber, clipper);
    }

    return cell;
}

void Registry::clip(
        const Id& index,
        const std::size_t chunkNum,
        const std::size_t id)
{
    m_cold->clip(index, chunkNum, id);
}

void Registry::save()
{
    m_base->save(m_endpoint);
    m_base.reset();
}

void Registry::merge(const Registry& other)
{
    if (m_cold && other.m_cold)
    {
        m_cold->merge(*other.m_cold);
    }

    if (m_base && other.m_base)
    {
        m_base->merge(*other.m_base);
    }
}

Json::Value Registry::toJson() const
{
    if (m_cold) return m_cold->toJson();
    else return Json::Value();
}

std::set<Id> Registry::ids() const
{
    if (m_cold) return m_cold->ids();
    else return std::set<Id>();
}

} // namespace entwine

