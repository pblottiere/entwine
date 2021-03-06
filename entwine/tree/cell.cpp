/******************************************************************************
* Copyright (c) 2016, Connor Manning (connor@hobu.co)
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#include <entwine/tree/cell.hpp>

namespace entwine
{

namespace
{
    const std::size_t unassigned(std::numeric_limits<std::size_t>::max());
}

Tube::Tube()
    : m_primaryTick(unassigned)
    , m_primaryCell()
    , m_cells()
    , m_mutex()
{ }

void Tube::addCell(const std::size_t tick, PooledInfoNode info)
{
    if (m_primaryTick == unassigned)
    {
        m_primaryTick = tick;
        m_primaryCell.store(info);
    }
    else
    {
        if (m_cells.count(tick))
        {
            throw std::runtime_error("Invalid serialized chunk tick");
        }

        m_cells.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(tick),
                std::forward_as_tuple(info));
    }
}

std::pair<bool, Cell&> Tube::getCell(const std::size_t tick)
{
    if (tick == m_primaryTick.load())
    {
        return std::pair<bool, Cell&>(false, m_primaryCell);
    }

    std::size_t unassignedMut(unassigned);
    const bool assigned(
            m_primaryTick.load() == unassigned &&
            m_primaryTick.compare_exchange_strong(unassignedMut, tick));

    // It's possible that another thread beat us to the swap, and successfully
    // swapped with the same value as our incoming tick.  Reload after the swap
    // attempt to counter this, since if our _assigned_ statement failed to
    // swap, then our primary tick is now set to its final value.
    if (assigned || m_primaryTick.load() == tick)
    {
        return std::pair<bool, Cell&>(assigned, m_primaryCell);
    }
    else
    {
        // Primary tick already assigned, and it's not the incoming one.  Go to
        // secondary cells.
        std::lock_guard<std::mutex> lock(m_mutex);
        const bool added(!m_cells.count(tick));
        return std::pair<bool, Cell&>(added, m_cells[tick]);
    }
}

bool Tube::empty() const
{
    return m_primaryTick.load() == unassigned;
}

void Tube::save(
        const Schema& schema,
        std::vector<char>& data,
        PooledDataStack& dataStack,
        PooledInfoStack& infoStack) const
{
    if (!empty())
    {
        const std::size_t pointSize(schema.pointSize());

        // Include space for primary cell.
        data.resize((m_cells.size() + 1) * pointSize);
        char* pos(data.data());
        const char* pointData(nullptr);

        const auto saveCell([&](const Cell& cell)
        {
            pointData = cell.atom().load()->val().data();
            std::copy(pointData, pointData + pointSize, pos);

            RawInfoNode* rawInfoNode(cell.atom().load());

            dataStack.push(rawInfoNode->val().acquireDataNode());
            infoStack.push(rawInfoNode);

            pos += pointSize;
        });

        saveCell(m_primaryCell);
        for (const auto& c : m_cells) saveCell(c.second);
    }
}

void Tube::saveBase(
        const Schema& celledSchema,
        const uint64_t tubeId,
        std::vector<char>& data,
        PooledDataStack& dataStack,
        PooledInfoStack& infoStack) const
{
    if (!empty())
    {
        const std::size_t idSize(sizeof(uint64_t));

        const std::size_t celledSize(celledSchema.pointSize());
        const std::size_t nativeSize(celledSize - idSize);

        // Include space for primary cell.
        data.resize((m_cells.size() + 1) * celledSize);
        char* pos(data.data());

        const char* tubePos(reinterpret_cast<const char*>(&tubeId));

        const auto saveCell([&](const Cell& cell)
        {
            const char* rawData(cell.atom().load()->val().data());

            std::copy(tubePos, tubePos + idSize, pos);
            std::copy(rawData, rawData + nativeSize, pos + idSize);

            RawInfoNode* rawInfoNode(cell.atom().load());

            dataStack.push(rawInfoNode->val().acquireDataNode());
            infoStack.push(rawInfoNode);

            pos += celledSize;
        });

        saveCell(m_primaryCell);
        for (const auto& c : m_cells) saveCell(c.second);
    }
}

PooledInfoStack Tube::acquire(InfoPool& infoPool)
{
    PooledInfoStack infoStack(infoPool);

    if (!empty())
    {
        infoStack.push(m_primaryCell.atom().load());
        for (const auto& c : m_cells) infoStack.push(c.second.atom().load());
    }

    return infoStack;
}

} // namespace entwine

