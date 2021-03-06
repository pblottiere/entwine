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

#include <cstddef>
#include <string>

#include <pdal/Dimension.hpp>

namespace entwine
{

class DimInfo
{
public:
    DimInfo(
            const std::string& name,
            pdal::Dimension::Id::Enum id,
            pdal::Dimension::Type::Enum type);
    DimInfo(
            const std::string& name,
            const std::string& baseTypeName,
            std::size_t size);

    std::string name() const;
    std::size_t size() const;
    std::string typeString() const;

    pdal::Dimension::Id::Enum id() const;
    pdal::Dimension::Type::Enum type() const;

    // May be unknown until PDAL registration.
    void setId(pdal::Dimension::Id::Enum id);

private:
    std::string m_name;
    pdal::Dimension::Id::Enum m_id;
    pdal::Dimension::Type::Enum m_type;
    std::string m_typeString;
};

using DimList = std::vector<DimInfo>;

inline bool operator==(const DimInfo& lhs, const DimInfo& rhs)
{
    return lhs.name() == rhs.name() && lhs.type() == rhs.type();
}

inline bool operator!=(const DimInfo& lhs, const DimInfo& rhs)
{
    return !(lhs == rhs);
}

} // namespace entwine

