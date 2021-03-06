/******************************************************************************
* Copyright (c) 2016, Connor Manning (connor@hobu.co)
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#pragma once

#include <pdal/Dimension.hpp>
#include <pdal/PointTable.hpp>

#include <entwine/tree/manifest.hpp>
#include <entwine/tree/point-info.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>

namespace entwine
{

class BinaryPointTable : public pdal::StreamPointTable
{
public:
    BinaryPointTable(const Schema& schema)
        : pdal::StreamPointTable(schema.pdalLayout())
        , m_pos(nullptr)
    { }

    BinaryPointTable(const Schema& schema, const char* pos)
        : pdal::StreamPointTable(schema.pdalLayout())
        , m_pos(pos)
    { }

    virtual pdal::point_count_t capacity() const override { return 1; }
    virtual char* getPoint(pdal::PointId i) override
    {
        // :(
        return const_cast<char*>(m_pos);
    }

    void setPoint(const char* pos) { m_pos = pos; }

protected:
    const char* m_pos;
};

class PooledPointTable : public pdal::StreamPointTable
{
public:
    // The processing function may acquire nodes from the incoming stack, and
    // can return any that do not need to be kept for reuse.
    PooledPointTable(
            PointPool& pointPool,
            std::function<PooledInfoStack(PooledInfoStack)> process,
            pdal::Dimension::Id::Enum originId = pdal::Dimension::Id::Unknown,
            Origin origin = invalidOrigin);

    virtual pdal::point_count_t capacity() const override;
    virtual void reset() override;

protected:
    virtual char* getPoint(pdal::PointId i) override
    {
        m_size = i + 1;
        return m_nodes[i]->val().data();
    }

private:
    void allocate();

    PointPool& m_pointPool;
    PooledInfoStack m_stack;
    std::deque<RawInfoNode*> m_nodes;   // m_nodes[0] -> m_stack.head()
    std::size_t m_size;

    std::function<PooledInfoStack(PooledInfoStack)> m_process;

    const pdal::Dimension::Id::Enum m_originId;
    const Origin m_origin;
};

} // namespace entwine

