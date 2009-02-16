/******************************************************************************
 * $Id$
 *
 * Project:  libLAS - http://liblas.org - A BSD library for LAS format data.
 * Purpose:  Reader implementation for C++ libLAS 
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2008, Mateusz Loskot
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following 
 * conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided 
 *       with the distribution.
 *     * Neither the name of the Martin Isenburg or Iowa Department 
 *       of Natural Resources nor the names of its contributors may be 
 *       used to endorse or promote products derived from this software 
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 ****************************************************************************/
 
#include <liblas/detail/reader.hpp>
#include <liblas/detail/reader10.hpp>
#include <liblas/detail/reader11.hpp>
#include <liblas/detail/reader12.hpp>
#include <liblas/lasheader.hpp>
#include <liblas/laspoint.hpp>

// GeoTIFF
#ifdef HAVE_LIBGEOTIFF
#include <geotiff.h>
#include <geo_simpletags.h>
#include "geo_normalize.h"
#include "geo_simpletags.h"
#include "geovalues.h"
#endif // HAVE_LIBGEOTIFF

// std
#include <fstream>
#include <cassert>
#include <cstdlib> // std::size_t
#include <stdexcept>
#include <iostream>

namespace liblas { namespace detail {

Reader::Reader(std::istream& ifs) : m_ifs(ifs), m_offset(0), m_current(0)
{
}

Reader::~Reader()
{
}

std::istream& Reader::GetStream() const
{
    return m_ifs;
}
void Reader::FillPoint(PointRecord& record, LASPoint& point) 
{
    
    point.SetX(record.x);
    point.SetY(record.y);
    point.SetZ(record.z);
    point.SetIntensity(record.intensity);
    point.SetScanFlags(record.flags);
    point.SetClassification(record.classification);
    point.SetScanAngleRank(record.scan_angle_rank);
    point.SetUserData(record.user_data);
    point.SetPointSourceID(record.point_source_id);
}


bool Reader::ReadVLR(LASHeader& header)
{
    VLRHeader vlrh = { 0 };

    m_ifs.seekg(header.GetHeaderSize(), std::ios::beg);
    uint32_t count = header.GetRecordsCount();
    header.SetRecordsCount(0);
    for (uint32_t i = 0; i < count; ++i)
    {
        read_n(vlrh, m_ifs, sizeof(VLRHeader));

        uint16_t length = vlrh.recordLengthAfterHeader;
        if (length < 1) {
            throw std::domain_error("VLR record length must be at least 1 byte long");
        }
         
        std::vector<uint8_t> data;
        data.resize(length);

        read_n(data.front(), m_ifs, length);
         
        LASVLR vlr;
        vlr.SetReserved(vlrh.reserved);
        vlr.SetUserId(std::string(vlrh.userId));
        vlr.SetDescription(std::string(vlrh.description));
        vlr.SetRecordLength(vlrh.recordLengthAfterHeader);
        vlr.SetRecordId(vlrh.recordId);
        vlr.SetData(data);

        header.AddVLR(vlr);
    }

    return true;
}

bool Reader::ReadGeoreference(LASHeader& header)
{
// #ifndef HAVE_LIBGEOTIFF
//     UNREFERENCED_PARAMETER(header);
// #else



    std::vector<LASVLR> vlrs;
    for (uint16_t i = 0; i < header.GetRecordsCount(); ++i)
    {
        LASVLR record = header.GetVLR(i);
        vlrs.push_back(record);
    }
    LASSRS* ref =  new LASSRS(vlrs);

    LASSRS srs(vlrs);
    
    header.SetSRS(srs);
    
    header.SetProj4(ref->GetProj4());
    
    std::cout << ref->GetWKT() << std::endl;
    
    delete ref;
        
        return true;

//#endif /* def HAVE_LIBGEOTIFF */

//    return false;
}

Reader* ReaderFactory::Create(std::istream& ifs)
{
    if (!ifs)
    {
        throw std::runtime_error("input stream state is invalid");
    }

    // Determine version of given LAS file and
    // instantiate appropriate reader.
    uint8_t verMajor = 0;
    uint8_t verMinor = 0;
    ifs.seekg(24, std::ios::beg);
    detail::read_n(verMajor, ifs, 1);
    detail::read_n(verMinor, ifs, 1);

    if (1 == verMajor && 0 == verMinor)
    {
        return new v10::ReaderImpl(ifs);
    }
    else if (1 == verMajor && 1 == verMinor)
    {
        return new v11::ReaderImpl(ifs);
    }
    else if (1 == verMajor && 2 == verMinor)
    {
        return new v12::ReaderImpl(ifs);
    }
    else if (2 == verMajor && 0 == verMinor )
    {
        // TODO: LAS 2.0 read/write support
        throw std::runtime_error("LAS 2.0+ file detected but unsupported");
    }

    throw std::runtime_error("LAS file of unknown version");
}

void ReaderFactory::Destroy(Reader* p) 
{
    delete p;
    p = 0;
}

}} // namespace liblas::detail
