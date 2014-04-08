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

#ifndef SNARK_WHEELS_WHEEL_COMMANDS_H_
#define SNARK_WHEELS_WHEEL_COMMANDS_H_

#include <boost/math/constants/constants.hpp>
#include <Eigen/Core>
#include <snark/math/frame_transforms.h>

namespace snark { namespace wheels {

const double turnrate_tolerance = 0.01; // rad/s

struct steer_command
{
    Eigen::Vector2d velocity;
    double turnrate; // radians

    steer_command() { }
    steer_command( const Eigen::Vector2d& velocity, const double turnrate ) : velocity( velocity ), turnrate( turnrate ) { }
    steer_command( const double x, const double y, const double turnrate ) : velocity( x, y ), turnrate( turnrate ) { }
};

struct wheel_command
{
    double velocity;
    double turnrate; // radians
};

/// returned turnrate is in radians
wheel_command compute_wheel_command( const steer_command& desired, Eigen::Matrix4d wheel_pose, double wheel_offset = 0 );

} } // namespace snark { namespace wheels {

#endif // SNARK_WHEELS_WHEEL_COMMANDS_H_