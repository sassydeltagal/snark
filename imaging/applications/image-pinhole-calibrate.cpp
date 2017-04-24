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
// 3. Neither the name of the University of Sydney nor the
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

/// @author vsevolod vlaskine

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <comma/application/command_line_options.h>
#include <comma/name_value/parser.h>
#include <comma/name_value/serialize.h>
#include "../camera/pinhole.h"
#include "../camera/traits.h"
#include "../cv_mat/serialization.h"

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

static void usage( bool verbose )
{
    std::cerr << std::endl;
    std::cerr << "perform intrinsic calibration of a camera based on a calibration pattern" << std::endl;
    std::cerr << "based on example at http://docs.opencv.org/2.4/doc/tutorials/calib3d/camera_calibration/camera_calibration.html" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: cat calibration_images.bin | image-pinhole-calibrate <options> > pinhole.json" << std::endl;
    std::cerr << "       where calibration-images.bin contains serialized calibration pattern images" << std::endl;
    if( verbose ) { std::cerr << snark::cv_mat::serialization::options::usage() << std::endl; }
    else { std::cerr << "run image-pinhole-calibrate --help --verbose for more details" << std::endl; }
    std::cerr << std::endl;
    std::cerr << "options" << std::endl;
    std::cerr << "    --verbose,-v: more output" << std::endl;
    std::cerr << "    --view: view detected calibration patterns, debugging option" << std::endl;
    std::cerr << std::endl;
    std::cerr << "calibration pattern options" << std::endl;
    std::cerr << "    --pattern-height,--height=<height>: number of inner corners per column on calibration pattern" << std::endl;
    std::cerr << "    --pattern-width,--width=<width>: number of inner corners per row on calibration pattern" << std::endl;
    std::cerr << "    --pattern-square-size,--square-size=<size>: size of a square on calibration board in user-defined metric (pixels, millimeters, etc)" << std::endl;
    std::cerr << "    --pattern=<pattern>: calibration pattern: chessboard, circles-grid, asymmetric-circles-grid" << std::endl;
    std::cerr << "                         default: chessboard" << std::endl;
    std::cerr << std::endl;
    std::cerr << "calibration options" << std::endl;
    std::cerr << "    --no-aspect-ratio: do not calibrate aspect ration, i.e. assume sensor pixels are exact squares" << std::endl;
    std::cerr << "    --no-principal-point: do not calibrate principal point, i.e. assume principal point is in the centre of the image" << std::endl;
    std::cerr << "    --no-tangential-distortion: do not calibrate tangential distortion, i.e. assume sensor plane is exactly perpendicular to camera axis" << std::endl;
    std::cerr << std::endl;
    std::cerr << "input options" << std::endl;
    std::cerr << "    --input=[<options>]: see --help --verbose for details" << std::endl;
    std::cerr << std::endl;
    exit( 0 );
}

struct output_t
{
    snark::camera::pinhole::config_t pinhole;
    double total_average_error;
    std::vector< std::string > offsets; // quick and dirty
    output_t() : total_average_error() {}
};

namespace comma { namespace visiting {

template <> struct traits< ::output_t >
{
    template < typename Key, class Visitor > static void visit( const Key&, const ::output_t& p, Visitor& v )
    {
        v.apply( "pinhole", p.pinhole );
        v.apply( "total_average_error", p.total_average_error );
        v.apply( "offsets", p.offsets );
    }    
};

} } // namespace comma { namespace visiting {

struct patterns { enum types { chessboard, circles_grid, asymmetric_circles_grid, invalid }; };

static std::vector< cv::Point3f > pattern_corners_( patterns::types pattern, const cv::Size& pattern_size, float square_size )
{
    std::vector< cv::Point3f > corners;
    for( int i = 0; i < pattern_size.height; ++i )
    {
        for( int j = 0; j < pattern_size.width; ++j )
        {
            corners.push_back( cv::Point3f( patterns::asymmetric_circles_grid ? ( 2 * j + i % 2 ) * square_size : j * square_size, i * square_size, 0 ) );
        }
    }
    return corners;
}

static double reprojection_error( const std::vector< std::vector< cv::Point3f > >& object_points
                                , const std::vector< std::vector< cv::Point2f > >& image_points
                                , const std::vector< cv::Mat >& rvecs
                                , const std::vector< cv::Mat >& tvecs
                                , const cv::Mat& camera_matrix
                                , const cv::Mat& distortion_coefficients
                                , std::vector< float >& per_view_errors )
{
    std::vector< cv::Point2f > image_points_2;
    int total_points = 0;
    double total_error = 0;
    per_view_errors.resize( object_points.size() );
    for( unsigned int i = 0; i < object_points.size(); ++i )
    {
        cv::projectPoints( cv::Mat( object_points[i] ), rvecs[i], tvecs[i], camera_matrix, distortion_coefficients, image_points_2 );
        double err = cv::norm( cv::Mat( image_points[i] ), cv::Mat( image_points_2 ), CV_L2 );
        unsigned int n = object_points[i].size();
        per_view_errors[i] = std::sqrt( err * err / n );
        total_error += err * err;
        total_points += n;
    }
    return std::sqrt( total_error / total_points );
}

static bool calibrate( const std::vector< cv::Point3f >& pattern_corners
                     , int flags
                     , const cv::Size& image_size
                     , const std::vector< std::vector< cv::Point2f > >& image_points
                     , cv::Mat& camera_matrix
                     , cv::Mat& distortion_coefficients
                     , std::vector< cv::Mat >& rvecs
                     , std::vector< cv::Mat >& tvecs
                     , std::vector< float >& reprojection_errors
                     , double& total_average_error )
{

    camera_matrix = cv::Mat::eye( 3, 3, CV_64F );
    if( flags & CV_CALIB_FIX_ASPECT_RATIO ) { camera_matrix.at< double >( 0, 0 ) = 1.0; }
    distortion_coefficients = cv::Mat::zeros( 8, 1, CV_64F );
    std::vector< std::vector< cv::Point3f > > object_points( 1 );
    object_points[0] = pattern_corners;
    object_points.resize( image_points.size(), object_points[0] );
    cv::calibrateCamera( object_points, image_points, image_size, camera_matrix, distortion_coefficients, rvecs, tvecs, flags | CV_CALIB_FIX_K4 | CV_CALIB_FIX_K5 );
    bool ok = cv::checkRange( camera_matrix ) && cv::checkRange( distortion_coefficients );
    total_average_error = reprojection_error( object_points, image_points, rvecs, tvecs, camera_matrix, distortion_coefficients, reprojection_errors );
    return ok;
}

int new_main( int ac, char** av )
{
    try
    {
        comma::command_line_options options( ac, av, usage );
        cv::Size pattern_size( options.value< int >( "--pattern-width,--width" ), options.value< int >( "--pattern-height,--height" ) );
        std::string pattern_string = options.value< std::string >( "--pattern", "chessboard" );
        patterns::types pattern = pattern_string == "chessboard" ? patterns::chessboard
                                : pattern_string == "circles-grid" ? patterns::circles_grid
                                : pattern_string == "asymmetric-circles-grid" ? patterns::asymmetric_circles_grid
                                : patterns::invalid;
        if( pattern == patterns::invalid ) { std::cerr << "image-pinhole-calibration: expected calibration pattern, got: \"" << pattern_string << "\"" << std::endl; }
        float square_size = options.value< float >( "--pattern-square-size,--square-size", 0 ); 
        int flags = 0;
        if( options.exists( "--no-principal-point" ) ) { flags |= CV_CALIB_FIX_PRINCIPAL_POINT; }
        if( options.exists( "--no-tangential-distortion" ) ) { flags |= CV_CALIB_ZERO_TANGENT_DIST; }
        if( options.exists( "--no-aspect-ratio" ) ) { flags |= CV_CALIB_FIX_ASPECT_RATIO; }
        bool view = options.exists( "--view" );
        std::vector< std::vector< cv::Point2f > > image_points;
        cv::Mat camera_matrix;
        cv::Mat distortion_coefficients;
        std::vector< cv::Mat > rvecs;
        std::vector< cv::Mat > tvecs;
        std::vector< float > reprojection_errors;
        static const cv::Scalar red( 0, 0, 255 );
        static const cv::Scalar green( 0, 255, 0 );
        snark::cv_mat::serialization input( comma::name_value::parser( ';', '=' ).get< snark::cv_mat::serialization::options >( options.value< std::string >( "--input", "" ) ) );
        std::vector< cv::Point3f > pattern_corners = pattern_corners_( pattern, pattern_size, square_size );
        std::pair< snark::cv_mat::serialization::header::buffer_t, cv::Mat > pair;
        while( !std::cin.eof() && std::cin.good() )
        {
            pair = input.read< snark::cv_mat::serialization::header::buffer_t >( std::cin );
            if( pair.second.empty() ) { break; }
            std::vector< cv::Point2f > points;
            bool found = pattern == patterns::chessboard ? cv::findChessboardCorners( pair.second, pattern_size, points, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE )
                       : pattern == patterns::circles_grid ? findCirclesGrid( view, pattern_size, points )
                       : pattern == patterns::asymmetric_circles_grid ? findCirclesGrid( view, pattern_size, points, cv::CALIB_CB_ASYMMETRIC_GRID )
                       : false;
            if( found )
            {
                if( pattern == patterns::chessboard ) // improve found corners
                {
                    cv::Mat grey;
                    cv::cvtColor( pair.second, grey, cv::COLOR_BGR2GRAY );
                    cv::cornerSubPix( grey, points, cv::Size( 11, 11 ), cv::Size( -1, -1 ), cv::TermCriteria( CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1 ) );
                }
                cv::drawChessboardCorners( pair.second, pattern_size, cv::Mat( points ), found );
            }
        }
        if( image_points.empty() ) { std::cerr << "image-pinhole-calibrate: did not get any image points" << std::endl; return 1; }
        ::output_t output;
        if( !calibrate( pattern_corners, flags, pair.second.size(), image_points, camera_matrix, distortion_coefficients, rvecs, tvecs, reprojection_errors, output.total_average_error ) ) { std::cerr << "image-pinhole-calibrate: calibration failed" << std::endl; return 1; }
        output.pinhole.image_size.x() = pair.second.size().width;
        output.pinhole.image_size.y() = pair.second.size().height;
        output.pinhole.principal_point = Eigen::Vector2d( camera_matrix.at< double >( 0, 2 ), camera_matrix.at< double >( 1, 2 ) );
        output.pinhole.focal_length; // todo
        output.pinhole.distortion = snark::camera::pinhole::config_t::distortion_t();
        output.pinhole.distortion->radial.k1 = distortion_coefficients.at< double >( 0 );
        output.pinhole.distortion->radial.k2 = distortion_coefficients.at< double >( 1 );
        output.pinhole.distortion->tangential.p1 = distortion_coefficients.at< double >( 2 );
        output.pinhole.distortion->tangential.p2 = distortion_coefficients.at< double >( 3 );
        output.pinhole.distortion->radial.k3 = distortion_coefficients.at< double >( 4 );
        comma::write_json( output, std::cout );
        return 0;
    }
    catch( std::exception& ex ) { std::cerr << "image-pinhole-calibration: " << ex.what() << std::endl; }
    catch( ... ) { std::cerr << "image-pinhole-calibration: unknown exception" << std::endl; }
    return 1;
}


using namespace cv;
using namespace std;

static void help()
{
    cout <<  "This is a camera calibration sample." << endl
         <<  "Usage: calibration configurationFile"  << endl
         <<  "Near the sample file you'll find the configuration file, which has detailed help of "
             "how to edit it.  It may be any OpenCV supported file format XML/YAML." << endl;
}
class Settings
{
public:
    Settings() : goodInput(false) {}
    enum Pattern { NOT_EXISTING, CHESSBOARD, CIRCLES_GRID, ASYMMETRIC_CIRCLES_GRID };
    enum InputType {INVALID, CAMERA, VIDEO_FILE, IMAGE_LIST};

    void write(FileStorage& fs) const                        //Write serialization for this class
    {
        fs << "{" << "BoardSize_Width"  << boardSize.width
                  << "BoardSize_Height" << boardSize.height
                  << "Square_Size"         << squareSize
                  << "Calibrate_Pattern" << patternToUse
                  << "Calibrate_NrOfFrameToUse" << nrFrames
                  << "Calibrate_FixAspectRatio" << aspectRatio
                  << "Calibrate_AssumeZeroTangentialDistortion" << calibZeroTangentDist
                  << "Calibrate_FixPrincipalPointAtTheCenter" << calibFixPrincipalPoint

                  << "Write_DetectedFeaturePoints" << bwritePoints
                  << "Write_extrinsicParameters"   << bwriteExtrinsics
                  << "Write_outputFileName"  << outputFileName

                  << "Show_UndistortedImage" << showUndistorsed

                  << "Input_FlipAroundHorizontalAxis" << flipVertical
                  << "Input_Delay" << delay
                  << "Input" << input
           << "}";
    }
    void read(const FileNode& node)                          //Read serialization for this class
    {
        node["BoardSize_Width" ] >> boardSize.width;
        node["BoardSize_Height"] >> boardSize.height;
        node["Calibrate_Pattern"] >> patternToUse;
        node["Square_Size"]  >> squareSize;
        node["Calibrate_NrOfFrameToUse"] >> nrFrames;
        node["Calibrate_FixAspectRatio"] >> aspectRatio;
        node["Write_DetectedFeaturePoints"] >> bwritePoints;
        node["Write_extrinsicParameters"] >> bwriteExtrinsics;
        node["Write_outputFileName"] >> outputFileName;
        node["Calibrate_AssumeZeroTangentialDistortion"] >> calibZeroTangentDist;
        node["Calibrate_FixPrincipalPointAtTheCenter"] >> calibFixPrincipalPoint;
        node["Input_FlipAroundHorizontalAxis"] >> flipVertical;
        node["Show_UndistortedImage"] >> showUndistorsed;
        node["Input"] >> input;
        node["Input_Delay"] >> delay;
        interprate();
    }
    void interprate()
    {
        goodInput = true;
        if (boardSize.width <= 0 || boardSize.height <= 0)
        {
            cerr << "Invalid Board size: " << boardSize.width << " " << boardSize.height << endl;
            goodInput = false;
        }
        if (squareSize <= 10e-6)
        {
            cerr << "Invalid square size " << squareSize << endl;
            goodInput = false;
        }
        if (nrFrames <= 0)
        {
            cerr << "Invalid number of frames " << nrFrames << endl;
            goodInput = false;
        }

        if (input.empty())      // Check for valid input
                inputType = INVALID;
        else
        {
            if (input[0] >= '0' && input[0] <= '9')
            {
                stringstream ss(input);
                ss >> cameraID;
                inputType = CAMERA;
            }
            else
            {
                if (isListOfImages(input) && readStringList(input, imageList))
                    {
                        inputType = IMAGE_LIST;
                        nrFrames = (nrFrames < (int)imageList.size()) ? nrFrames : (int)imageList.size();
                    }
                else
                    inputType = VIDEO_FILE;
            }
            if (inputType == CAMERA)
                inputCapture.open(cameraID);
            if (inputType == VIDEO_FILE)
                inputCapture.open(input);
            if (inputType != IMAGE_LIST && !inputCapture.isOpened())
                    inputType = INVALID;
        }
        if (inputType == INVALID)
        {
            cerr << " Inexistent input: " << input;
            goodInput = false;
        }

        flag = 0;
        if(calibFixPrincipalPoint) flag |= CV_CALIB_FIX_PRINCIPAL_POINT;
        if(calibZeroTangentDist)   flag |= CV_CALIB_ZERO_TANGENT_DIST;
        if(aspectRatio)            flag |= CV_CALIB_FIX_ASPECT_RATIO;


        calibrationPattern = NOT_EXISTING;
        if (!patternToUse.compare("CHESSBOARD")) calibrationPattern = CHESSBOARD;
        if (!patternToUse.compare("CIRCLES_GRID")) calibrationPattern = CIRCLES_GRID;
        if (!patternToUse.compare("ASYMMETRIC_CIRCLES_GRID")) calibrationPattern = ASYMMETRIC_CIRCLES_GRID;
        if (calibrationPattern == NOT_EXISTING)
            {
                cerr << " Inexistent camera calibration mode: " << patternToUse << endl;
                goodInput = false;
            }
        atImageList = 0;

    }
    Mat nextImage()
    {
        Mat result;
        if( inputCapture.isOpened() )
        {
            Mat view0;
            inputCapture >> view0;
            view0.copyTo(result);
        }
        else if( atImageList < (int)imageList.size() )
            result = imread(imageList[atImageList++], CV_LOAD_IMAGE_COLOR);

        return result;
    }

    static bool readStringList( const string& filename, vector<string>& l )
    {
        l.clear();
        FileStorage fs(filename, FileStorage::READ);
        if( !fs.isOpened() )
            return false;
        FileNode n = fs.getFirstTopLevelNode();
        if( n.type() != FileNode::SEQ )
            return false;
        FileNodeIterator it = n.begin(), it_end = n.end();
        for( ; it != it_end; ++it )
            l.push_back((string)*it);
        return true;
    }

    static bool isListOfImages( const string& filename)
    {
        string s(filename);
        // Look for file extension
        if( s.find(".xml") == string::npos && s.find(".yaml") == string::npos && s.find(".yml") == string::npos )
            return false;
        else
            return true;
    }
public:
    Size boardSize;            // The size of the board -> Number of items by width and height
    Pattern calibrationPattern;// One of the Chessboard, circles, or asymmetric circle pattern
    float squareSize;          // The size of a square in your defined unit (point, millimeter,etc).
    int nrFrames;              // The number of frames to use from the input for calibration
    float aspectRatio;         // The aspect ratio
    int delay;                 // In case of a video input
    bool bwritePoints;         //  Write detected feature points
    bool bwriteExtrinsics;     // Write extrinsic parameters
    bool calibZeroTangentDist; // Assume zero tangential distortion
    bool calibFixPrincipalPoint;// Fix the principal point at the center
    bool flipVertical;          // Flip the captured images around the horizontal axis
    string outputFileName;      // The name of the file where to write
    bool showUndistorsed;       // Show undistorted images after calibration
    string input;               // The input ->



    int cameraID;
    vector<string> imageList;
    int atImageList;
    VideoCapture inputCapture;
    InputType inputType;
    bool goodInput;
    int flag;

private:
    string patternToUse;


};

static void read(const FileNode& node, Settings& x, const Settings& default_value = Settings())
{
    if(node.empty())
        x = default_value;
    else
        x.read(node);
}

enum { DETECTION = 0, CAPTURING = 1, CALIBRATED = 2 };

bool runCalibrationAndSave(Settings& s, Size imageSize, Mat&  cameraMatrix, Mat& distCoeffs,
                           vector<vector<Point2f> > imagePoints );

int main(int argc, char* argv[])
{
    help();
    Settings s;
    const string inputSettingsFile = argc > 1 ? argv[1] : "default.xml";
    FileStorage fs(inputSettingsFile, FileStorage::READ); // Read the settings
    if (!fs.isOpened())
    {
        cout << "Could not open the configuration file: \"" << inputSettingsFile << "\"" << endl;
        return -1;
    }
    fs["Settings"] >> s;
    fs.release();                                         // close Settings file

    if (!s.goodInput)
    {
        cout << "Invalid input detected. Application stopping. " << endl;
        return -1;
    }

    vector<vector<Point2f> > imagePoints;
    Mat cameraMatrix, distCoeffs;
    Size imageSize;
    int mode = s.inputType == Settings::IMAGE_LIST ? CAPTURING : DETECTION;
    clock_t prevTimestamp = 0;
    const Scalar RED(0,0,255), GREEN(0,255,0);
    const char ESC_KEY = 27;

    for(int i = 0;;++i)
    {
      Mat view;
      bool blinkOutput = false;

      view = s.nextImage();

      //-----  If no more image, or got enough, then stop calibration and show result -------------
      if( mode == CAPTURING && imagePoints.size() >= (unsigned)s.nrFrames )
      {
          if( runCalibrationAndSave(s, imageSize,  cameraMatrix, distCoeffs, imagePoints))
              mode = CALIBRATED;
          else
              mode = DETECTION;
      }
      if(view.empty())          // If no more images then run calibration, save and stop loop.
      {
            if( imagePoints.size() > 0 )
                runCalibrationAndSave(s, imageSize,  cameraMatrix, distCoeffs, imagePoints);
            break;
      }


        imageSize = view.size();  // Format input image.
        if( s.flipVertical )    flip( view, view, 0 );

        vector<Point2f> pointBuf;

        bool found;
        switch( s.calibrationPattern ) // Find feature points on the input format
        {
        case Settings::CHESSBOARD:
            found = findChessboardCorners( view, s.boardSize, pointBuf,
                CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE);
            break;
        case Settings::CIRCLES_GRID:
            found = findCirclesGrid( view, s.boardSize, pointBuf );
            break;
        case Settings::ASYMMETRIC_CIRCLES_GRID:
            found = findCirclesGrid( view, s.boardSize, pointBuf, CALIB_CB_ASYMMETRIC_GRID );
            break;
        default:
            found = false;
            break;
        }

        if ( found)                // If done with success,
        {
              // improve the found corners' coordinate accuracy for chessboard
                if( s.calibrationPattern == Settings::CHESSBOARD)
                {
                    Mat viewGray;
                    cvtColor(view, viewGray, COLOR_BGR2GRAY);
                    cornerSubPix( viewGray, pointBuf, Size(11,11),
                        Size(-1,-1), TermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));
                }

                if( mode == CAPTURING &&  // For camera only take new samples after delay time
                    (!s.inputCapture.isOpened() || clock() - prevTimestamp > s.delay*1e-3*CLOCKS_PER_SEC) )
                {
                    imagePoints.push_back(pointBuf);
                    prevTimestamp = clock();
                    blinkOutput = s.inputCapture.isOpened();
                }

                // Draw the corners.
                drawChessboardCorners( view, s.boardSize, Mat(pointBuf), found );
        }

        //----------------------------- Output Text ------------------------------------------------
        string msg = (mode == CAPTURING) ? "100/100" :
                      mode == CALIBRATED ? "Calibrated" : "Press 'g' to start";
        int baseLine = 0;
        Size textSize = getTextSize(msg, 1, 1, 1, &baseLine);
        Point textOrigin(view.cols - 2*textSize.width - 10, view.rows - 2*baseLine - 10);

        if( mode == CAPTURING )
        {
            if(s.showUndistorsed)
                msg = format( "%d/%d Undist", (int)imagePoints.size(), s.nrFrames );
            else
                msg = format( "%d/%d", (int)imagePoints.size(), s.nrFrames );
        }

        putText( view, msg, textOrigin, 1, 1, mode == CALIBRATED ?  GREEN : RED);

        if( blinkOutput )
            bitwise_not(view, view);

        //------------------------- Video capture  output  undistorted ------------------------------
        if( mode == CALIBRATED && s.showUndistorsed )
        {
            Mat temp = view.clone();
            undistort(temp, view, cameraMatrix, distCoeffs);
        }

        //------------------------------ Show image and check for input commands -------------------
        imshow("Image View", view);
        char key = (char)waitKey(s.inputCapture.isOpened() ? 50 : s.delay);

        if( key  == ESC_KEY )
            break;

        if( key == 'u' && mode == CALIBRATED )
           s.showUndistorsed = !s.showUndistorsed;

        if( s.inputCapture.isOpened() && key == 'g' )
        {
            mode = CAPTURING;
            imagePoints.clear();
        }
    }

    // -----------------------Show the undistorted image for the image list ------------------------
    if( s.inputType == Settings::IMAGE_LIST && s.showUndistorsed )
    {
        Mat view, rview, map1, map2;
        initUndistortRectifyMap(cameraMatrix, distCoeffs, Mat(),
            getOptimalNewCameraMatrix(cameraMatrix, distCoeffs, imageSize, 1, imageSize, 0),
            imageSize, CV_16SC2, map1, map2);

        for(int i = 0; i < (int)s.imageList.size(); i++ )
        {
            view = imread(s.imageList[i], 1);
            if(view.empty())
                continue;
            remap(view, rview, map1, map2, INTER_LINEAR);
            imshow("Image View", rview);
            char c = (char)waitKey();
            if( c  == ESC_KEY || c == 'q' || c == 'Q' )
                break;
        }
    }


    return 0;
}

static double computeReprojectionErrors( const vector<vector<Point3f> >& objectPoints,
                                         const vector<vector<Point2f> >& imagePoints,
                                         const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                                         const Mat& cameraMatrix , const Mat& distCoeffs,
                                         vector<float>& perViewErrors)
{
    vector<Point2f> imagePoints2;
    int i, totalPoints = 0;
    double totalErr = 0, err;
    perViewErrors.resize(objectPoints.size());

    for( i = 0; i < (int)objectPoints.size(); ++i )
    {
        projectPoints( Mat(objectPoints[i]), rvecs[i], tvecs[i], cameraMatrix,
                       distCoeffs, imagePoints2);
        err = norm(Mat(imagePoints[i]), Mat(imagePoints2), CV_L2);

        int n = (int)objectPoints[i].size();
        perViewErrors[i] = (float) std::sqrt(err*err/n);
        totalErr        += err*err;
        totalPoints     += n;
    }

    return std::sqrt(totalErr/totalPoints);
}

static void calcBoardCornerPositions(Size boardSize, float squareSize, vector<Point3f>& corners,
                                     Settings::Pattern patternType /*= Settings::CHESSBOARD*/)
{
    corners.clear();

    switch(patternType)
    {
    case Settings::CHESSBOARD:
    case Settings::CIRCLES_GRID:
        for( int i = 0; i < boardSize.height; ++i )
            for( int j = 0; j < boardSize.width; ++j )
                corners.push_back(Point3f(float( j*squareSize ), float( i*squareSize ), 0));
        break;

    case Settings::ASYMMETRIC_CIRCLES_GRID:
        for( int i = 0; i < boardSize.height; i++ )
            for( int j = 0; j < boardSize.width; j++ )
                corners.push_back(Point3f(float((2*j + i % 2)*squareSize), float(i*squareSize), 0));
        break;
    default:
        break;
    }
}

static bool runCalibration( Settings& s, Size& imageSize, Mat& cameraMatrix, Mat& distCoeffs,
                            vector<vector<Point2f> > imagePoints, vector<Mat>& rvecs, vector<Mat>& tvecs,
                            vector<float>& reprojErrs,  double& totalAvgErr)
{

    cameraMatrix = Mat::eye(3, 3, CV_64F);
    if( s.flag & CV_CALIB_FIX_ASPECT_RATIO )
        cameraMatrix.at<double>(0,0) = 1.0;

    distCoeffs = Mat::zeros(8, 1, CV_64F);

    vector<vector<Point3f> > objectPoints(1);
    calcBoardCornerPositions(s.boardSize, s.squareSize, objectPoints[0], s.calibrationPattern);

    objectPoints.resize(imagePoints.size(),objectPoints[0]);

    //Find intrinsic and extrinsic camera parameters
    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix,
                                 distCoeffs, rvecs, tvecs, s.flag|CV_CALIB_FIX_K4|CV_CALIB_FIX_K5);

    cout << "Re-projection error reported by calibrateCamera: "<< rms << endl;

    bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);

    totalAvgErr = computeReprojectionErrors(objectPoints, imagePoints,
                                             rvecs, tvecs, cameraMatrix, distCoeffs, reprojErrs);

    return ok;
}

// Print camera parameters to the output file
static void saveCameraParams( Settings& s, Size& imageSize, Mat& cameraMatrix, Mat& distCoeffs,
                              const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                              const vector<float>& reprojErrs, const vector<vector<Point2f> >& imagePoints,
                              double totalAvgErr )
{
    FileStorage fs( s.outputFileName, FileStorage::WRITE );

    time_t tm;
    time( &tm );
    struct tm *t2 = localtime( &tm );
    char buf[1024];
    strftime( buf, sizeof(buf)-1, "%c", t2 );

    fs << "calibration_Time" << buf;

    if( !rvecs.empty() || !reprojErrs.empty() )
        fs << "nrOfFrames" << (int)std::max(rvecs.size(), reprojErrs.size());
    fs << "image_Width" << imageSize.width;
    fs << "image_Height" << imageSize.height;
    fs << "board_Width" << s.boardSize.width;
    fs << "board_Height" << s.boardSize.height;
    fs << "square_Size" << s.squareSize;

    if( s.flag & CV_CALIB_FIX_ASPECT_RATIO )
        fs << "FixAspectRatio" << s.aspectRatio;

    if( s.flag )
    {
        sprintf( buf, "flags: %s%s%s%s",
            s.flag & CV_CALIB_USE_INTRINSIC_GUESS ? " +use_intrinsic_guess" : "",
            s.flag & CV_CALIB_FIX_ASPECT_RATIO ? " +fix_aspectRatio" : "",
            s.flag & CV_CALIB_FIX_PRINCIPAL_POINT ? " +fix_principal_point" : "",
            s.flag & CV_CALIB_ZERO_TANGENT_DIST ? " +zero_tangent_dist" : "" );
        cvWriteComment( *fs, buf, 0 );

    }

    fs << "flagValue" << s.flag;

    fs << "Camera_Matrix" << cameraMatrix;
    fs << "Distortion_Coefficients" << distCoeffs;

    fs << "Avg_Reprojection_Error" << totalAvgErr;
    if( !reprojErrs.empty() )
        fs << "Per_View_Reprojection_Errors" << Mat(reprojErrs);

    if( !rvecs.empty() && !tvecs.empty() )
    {
        CV_Assert(rvecs[0].type() == tvecs[0].type());
        Mat bigmat((int)rvecs.size(), 6, rvecs[0].type());
        for( int i = 0; i < (int)rvecs.size(); i++ )
        {
            Mat r = bigmat(Range(i, i+1), Range(0,3));
            Mat t = bigmat(Range(i, i+1), Range(3,6));

            CV_Assert(rvecs[i].rows == 3 && rvecs[i].cols == 1);
            CV_Assert(tvecs[i].rows == 3 && tvecs[i].cols == 1);
            //*.t() is MatExpr (not Mat) so we can use assignment operator
            r = rvecs[i].t();
            t = tvecs[i].t();
        }
        cvWriteComment( *fs, "a set of 6-tuples (rotation vector + translation vector) for each view", 0 );
        fs << "Extrinsic_Parameters" << bigmat;
    }

    if( !imagePoints.empty() )
    {
        Mat imagePtMat((int)imagePoints.size(), (int)imagePoints[0].size(), CV_32FC2);
        for( int i = 0; i < (int)imagePoints.size(); i++ )
        {
            Mat r = imagePtMat.row(i).reshape(2, imagePtMat.cols);
            Mat imgpti(imagePoints[i]);
            imgpti.copyTo(r);
        }
        fs << "Image_points" << imagePtMat;
    }
}

bool runCalibrationAndSave(Settings& s, Size imageSize, Mat&  cameraMatrix, Mat& distCoeffs,vector<vector<Point2f> > imagePoints )
{
    vector<Mat> rvecs, tvecs;
    vector<float> reprojErrs;
    double totalAvgErr = 0;

    bool ok = runCalibration(s,imageSize, cameraMatrix, distCoeffs, imagePoints, rvecs, tvecs,
                             reprojErrs, totalAvgErr);
    cout << (ok ? "Calibration succeeded" : "Calibration failed")
        << ". avg re projection error = "  << totalAvgErr ;

    if( ok )
        saveCameraParams( s, imageSize, cameraMatrix, distCoeffs, rvecs ,tvecs, reprojErrs,
                            imagePoints, totalAvgErr);
    return ok;
}
