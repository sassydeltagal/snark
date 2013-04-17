// This file is part of snark, a generic and flexible library for robotics research
// Copyright (c) 2011 The University of Sydney
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All advertising materials mentioning features or use of this software
//    must display the following acknowledgement:
//    This product includes software developed by the The University of Sydney.
// 4. Neither the name of the The University of Sydney nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
// HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <gtest/gtest.h>
#include <cmath>
#include <snark/point_cloud/spherical_grid.h>

namespace snark {

TEST( spherical_grid, bearing_elevation_index_usage_example )
{
    bearing_elevation begin( -M_PI, -M_PI / 2 );
    bearing_elevation resolution( 0.1, 0.02 );
    bearing_elevation_grid::index index( begin, resolution );
    bearing_elevation_grid::index::type size = index( bearing_elevation( 1.2, 0.6 ) );
    boost::multi_array< std::string, 2 > grid( boost::extents[size[0]][size[1]] );
    bearing_elevation_grid::index::type some_point = index( bearing_elevation( 1.1, 0.5 ) );
    grid( some_point ) = "hello world";
}

static const double one_degree = M_PI / 180;

TEST( spherical_grid, bearing_elevation_gid_usage_example )
{
    bearing_elevation_grid::type< std::string > grid( bearing_elevation_grid::index( one_degree ), 360, 180 );
    grid( 20 * one_degree, 30 * one_degree ) = "hello world";
    EXPECT_EQ( "hello world", grid( 20 * one_degree, 30 * one_degree ) );
    EXPECT_EQ( "", grid( 19.5 * one_degree, 30 * one_degree ) );
}

TEST( spherical_grid, bearing_index )
{
    for( int i = -180; i < 180; ++i )
    {
        EXPECT_EQ( i + 180, bearing_elevation_grid::index( one_degree )( one_degree * i, 0 )[0] );
        EXPECT_EQ( i + 180, bearing_elevation_grid::index( one_degree )( one_degree * ( 0.5 + i ), 0 )[0] );
    }
    // todo: test with arbitrary begin
}

TEST( spherical_grid, elevation_index )
{
    for( int i = -90; i < 90; ++i )
    {
        EXPECT_EQ( i + 90, bearing_elevation_grid::index( one_degree )( 0, one_degree * i )[1] );
        EXPECT_EQ( i + 90, bearing_elevation_grid::index( one_degree )( 0, one_degree * ( 0.5 + i ) )[1] );
    }
    // todo: test with arbitrary begin
}

} // namespace snark {
