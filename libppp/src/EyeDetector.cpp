#include "LandMarks.h"
#include "EyeDetector.h"
#include "Geometry.h"
#include "CommonHelpers.h"

#include <queue>

#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

void EyeDetector::configure(rapidjson::Value& cfg)
{
    auto& edCfg = cfg["eyesDetector"];

    createCornerKernels();

    m_useHaarCascades = edCfg["useHaarCascade"].GetBool();

    if (m_useHaarCascades)
    {
        auto loadCascade = [&edCfg](const string& eyeName)
        {
            auto & haarCascade = edCfg[(string("haarCascade") + eyeName).c_str()];
            auto xmlBase64Data(haarCascade["data"].GetString());
            return CommonHelpers::loadClassifierFromBase64(xmlBase64Data);
        };
        m_leftEyeCascadeClassifier = loadCascade("Left");
        m_rightEyeCascadeClassifier = loadCascade("Right");
    }
}

bool EyeDetector::detectLandMarks(const cv::Mat& grayImage, LandMarks& landMarks)
{
    const auto& faceRect = landMarks.vjFaceRect;

    if (faceRect.width <= 10 && faceRect.height <=10)
    {
        throw std::runtime_error("Face rectangle is too small or not defined");
    }

    auto faceImage = grayImage(faceRect);

    if (kSmoothFaceImage)
    {
        double sigma = kSmoothFaceFactor * faceRect.width;
        GaussianBlur(faceImage, faceImage, cv::Size(0, 0), sigma);
    }
    //-- Find eye regions and draw them
    auto eyeRegionWidth = ROUND_INT(faceRect.width * m_widthRatio);
    auto eyeRegionHeight = ROUND_INT(faceRect.width * m_heightRatio);
    auto eyeRegionTop = ROUND_INT(faceRect.height * m_topFaceRatio);
    auto eyeRegionLeft = ROUND_INT(faceRect.width* m_sideFaceRatio);


    cv::Rect leftEyeRegion(eyeRegionLeft, eyeRegionTop, eyeRegionWidth, eyeRegionHeight);
    cv::Rect rightEyeRegion(faceRect.width - eyeRegionWidth - eyeRegionLeft,
                        eyeRegionTop, eyeRegionWidth, eyeRegionHeight);

    auto leftEyeImage = faceImage(leftEyeRegion);
    auto rightEyeImage = faceImage(rightEyeRegion);
    if (m_useHaarCascades)
    {
        auto leftEyeHaarRect =
            detectWithHaarCascadeClassifier(leftEyeImage, m_leftEyeCascadeClassifier.get());
        auto rightEyeHaarRect =
            detectWithHaarCascadeClassifier(rightEyeImage, m_rightEyeCascadeClassifier.get());

        landMarks.vjLeftEyeRect = leftEyeHaarRect;
        landMarks.vjRightEyeRect = rightEyeHaarRect;

        landMarks.vjLeftEyeRect.x += faceRect.x + leftEyeRegion.x;
        landMarks.vjLeftEyeRect.y += faceRect.y + leftEyeRegion.y;

        landMarks.vjRightEyeRect.x += faceRect.x + rightEyeRegion.x;
        landMarks.vjRightEyeRect.y += faceRect.y + rightEyeRegion.y;


        if (leftEyeHaarRect.width > 0 && leftEyeHaarRect.height > 0)
        {
            // Reduce the search area for the pupils
            leftEyeRegion.x += leftEyeHaarRect.x;
            leftEyeRegion.y += leftEyeHaarRect.y;
            leftEyeRegion.width = leftEyeHaarRect.width;
            leftEyeRegion.height = leftEyeHaarRect.height;
        }
        if (rightEyeHaarRect.width > 0 && rightEyeHaarRect.height > 0)
        {
            rightEyeRegion.x += rightEyeHaarRect.x;
            rightEyeRegion.y += rightEyeHaarRect.y;
            rightEyeRegion.width = rightEyeHaarRect.width;
            rightEyeRegion.height = rightEyeHaarRect.height;
        }
    }

    //-- Find Eye Centers
    auto leftEyeCenter = findEyeCenter(faceImage(leftEyeRegion));
    auto rightEyeCenter = findEyeCenter(faceImage(rightEyeRegion));

    //-- If eye center touches or is very close to the eye ROI apply fallback method
    validateAndApplyFallbackIfRequired(leftEyeRegion.size(), leftEyeCenter);
    validateAndApplyFallbackIfRequired(rightEyeRegion.size(), rightEyeCenter);

    // Change eye centers to face coordinates
    rightEyeCenter.x += rightEyeRegion.x + faceRect.x;
    rightEyeCenter.y += rightEyeRegion.y + faceRect.y;
    leftEyeCenter.x += leftEyeRegion.x + faceRect.x;
    leftEyeCenter.y += leftEyeRegion.y + faceRect.y;


    //-- Find Eye Corners
    if (kEnableEyeCorner)
    {
        // Get corner regions
        cv::Rect leftRightCornerRegion(leftEyeRegion);
        leftRightCornerRegion.width -= leftEyeCenter.x;
        leftRightCornerRegion.x += leftEyeCenter.x;
        leftRightCornerRegion.height /= 2;
        leftRightCornerRegion.y += leftRightCornerRegion.height / 2;
        cv::Rect leftLeftCornerRegion(leftEyeRegion);
        leftLeftCornerRegion.width = leftEyeCenter.x;
        leftLeftCornerRegion.height /= 2;
        leftLeftCornerRegion.y += leftLeftCornerRegion.height / 2;
        cv::Rect rightLeftCornerRegion(rightEyeRegion);
        rightLeftCornerRegion.width = rightEyeCenter.x;
        rightLeftCornerRegion.height /= 2;
        rightLeftCornerRegion.y += rightLeftCornerRegion.height / 2;
        cv::Rect rightRightCornerRegion(rightEyeRegion);
        rightRightCornerRegion.width -= rightEyeCenter.x;
        rightRightCornerRegion.x += rightEyeCenter.x;
        rightRightCornerRegion.height /= 2;
        rightRightCornerRegion.y += rightRightCornerRegion.height / 2;

        cv::Point2f leftRightCorner = findEyeCorner(faceImage(leftRightCornerRegion), true, false);
        leftRightCorner.x += leftRightCornerRegion.x;
        leftRightCorner.y += leftRightCornerRegion.y;
        cv::Point2f leftLeftCorner = findEyeCorner(faceImage(leftLeftCornerRegion), true, true);
        leftLeftCorner.x += leftLeftCornerRegion.x;
        leftLeftCorner.y += leftLeftCornerRegion.y;
        cv::Point2f rightLeftCorner = findEyeCorner(faceImage(rightLeftCornerRegion), false, true);
        rightLeftCorner.x += rightLeftCornerRegion.x;
        rightLeftCorner.y += rightLeftCornerRegion.y;
        cv::Point2f rightRightCorner = findEyeCorner(faceImage(rightRightCornerRegion), false, false);
        rightRightCorner.x += rightRightCornerRegion.x;
        rightRightCorner.y += rightRightCornerRegion.y;
        circle(faceImage, leftRightCorner, 3, 200);
        circle(faceImage, leftLeftCorner, 3, 200);
        circle(faceImage, rightLeftCorner, 3, 200);
        circle(faceImage, rightRightCorner, 3, 200);
    }

    landMarks.eyeLeftPupil = leftEyeCenter;
    landMarks.eyeRightPupil = rightEyeCenter;

    return true;
}

void EyeDetector::validateAndApplyFallbackIfRequired(const cv::Size &eyeRoiSize, cv::Point &eyeCenter)
{
    if (eyeRoiSize.width <= eyeCenter.x || eyeRoiSize.height < eyeCenter.y)
    {
        throw std::logic_error("Detected eye position is outside the specifiied eye ROI");
    }

    const auto epsilon = std::min(eyeRoiSize.width, eyeRoiSize.height) * 0.05;
    
    if (eyeRoiSize.width- eyeCenter.x < epsilon || eyeCenter.x < epsilon 
        || eyeRoiSize.height - eyeCenter.y < epsilon || eyeCenter.y < epsilon)
    {
        eyeCenter.x = eyeRoiSize.width / 2;
        eyeCenter.y = eyeRoiSize.height / 2;
    }
}

cv::Rect EyeDetector::detectWithHaarCascadeClassifier(const cv::Mat &image, cv::CascadeClassifier* cc)
{
    vector<cv::Rect> results;
    cc->detectMultiScale(image, results, 1.05, 3,
                         CV_HAAR_SCALE_IMAGE | CV_HAAR_FIND_BIGGEST_OBJECT);
    if (results.empty() || results.size() > 1)
    {
        return cv::Rect();
    }
    return results.front();
}

void EyeDetector::createCornerKernels()
{
    m_rightCornerKernel = (cv::Mat_<float>(4, 6) <<
        -1, -1, -1,  1, 1, 1,
        -1, -1, -1, -1, 1, 1,
        -1, -1, -1, -1, 0, 3,
         1,  1,  1,  1, 1, 1);
    m_xGradKernel = (cv::Mat_<float>(3, 3) << 0, 0, 0, -0.5, 0, 0.5, 0, 0, 0);

    m_yGradKernel = m_xGradKernel.t();

    // flip horizontally
    cv::flip(m_rightCornerKernel, m_leftCornerKernel, 1);
}

cv::Point EyeDetector::findEyeCenter(const cv::Mat& eyeROIUnscaled)
{
    cv::Mat eyeROI; 
    scaleToFastSize(eyeROIUnscaled, eyeROI);

    //-- Find the gradient
    cv::Mat gradientX, gradientY;

    cv::filter2D(eyeROI, gradientX, CV_64F, m_xGradKernel);
    cv::filter2D(eyeROI, gradientY, CV_64F, m_yGradKernel);

    //-- Normalize and threshold the gradient, compute all the magnitudes
    cv::Mat mags = matrixMagnitude(gradientX, gradientY);
    //compute the threshold
    double gradientThresh = computeDynamicThreshold(mags, kGradientThreshold);
    //double gradientThresh = kGradientThreshold;
    //double gradientThresh = 0;
    //normalize
    for (auto y = 0; y < eyeROI.rows; ++y)
    {
        double* Xr = gradientX.ptr<double>(y);
        double* Yr = gradientY.ptr<double>(y);
        const double* Mr = mags.ptr<double>(y);

        for (int x = 0; x < eyeROI.cols; ++x)
        {
            double gX = Xr[x], gY = Yr[x];
            double magnitude = Mr[x];
            if (magnitude > gradientThresh)
            {
                Xr[x] = gX / magnitude;
                Yr[x] = gY / magnitude;
            }
            else
            {
                Xr[x] = 0.0;
                Yr[x] = 0.0;
            }
        }
    }

    //-- Create a blurred and inverted image for weighting
    cv::Mat weight;
    GaussianBlur(eyeROI, weight, cv::Size(kWeightBlurSize, kWeightBlurSize), 0, 0);

    weight = -weight + 255;

    //-- Run the algorithm!
    cv::Mat outSum = cv::Mat::zeros(eyeROI.rows, eyeROI.cols, CV_64F);
    // for each possible gradient location
    // Note: these loops are reversed from the way the paper does them
    // it evaluates every possible center for each gradient location instead of
    // every possible gradient location for every center.

    for (int y = 0; y < weight.rows; ++y)
    {
        const unsigned char* Wr = weight.ptr<unsigned char>(y);
        const double *Xr = gradientX.ptr<double>(y),
        *Yr = gradientY.ptr<double>(y);
        for (int x = 0; x < weight.cols; ++x)
        {
            double gX = Xr[x], gY = Yr[x];
            if (gX == 0.0 && gY == 0.0)
            {
                continue;
            }
            testPossibleCentersFormula(x, y, Wr[x], gX, gY, outSum);
        }
    }
    // scale all the values down, basically averaging them
    double numGradients = (weight.rows * weight.cols);
    cv::Mat out;
    outSum.convertTo(out, CV_32F, 1.0 / numGradients);

    //-- Find the maximum point
    cv::Point maxP;
    double maxVal;
    cv::minMaxLoc(out, nullptr, &maxVal, nullptr, &maxP);

    //-- Flood fill the edges
    if (kEnablePostProcess)
    {
        cv::Mat floodClone;
        //double floodThresh = computeDynamicThreshold(out, 1.5);
        double floodThresh = maxVal * kPostProcessThreshold;
        cv::threshold(out, floodClone, floodThresh, 0.0f, cv::THRESH_TOZERO);

        cv::Mat mask = floodKillEdges(floodClone);
        // redo max
        cv::minMaxLoc(out, nullptr, &maxVal, nullptr, &maxP, mask);
    }
    return unscalePoint(maxP, cv::Rect(cv::Point(0, 0), eyeROIUnscaled.size()));
}

cv::Mat EyeDetector::eyeCornerMap(const cv::Mat& region, bool left, bool left2) const
{
    cv::Mat cornerMap;

    auto sizeRegion = region.size();
    cv::Range colRange(sizeRegion.width / 4, sizeRegion.width * 3 / 4);
    cv::Range rowRange(sizeRegion.height / 4, sizeRegion.height * 3 / 4);

    cv::Mat miRegion(region, rowRange, colRange);

    cv::filter2D(miRegion, cornerMap, CV_32F,
        (left && !left2) || (!left && !left2) ? m_leftCornerKernel : m_rightCornerKernel);
    return cornerMap;
}

cv::Point2f EyeDetector::findEyeCorner(cv::Mat region, bool left, bool left2) const
{
    auto cornerMap = eyeCornerMap(region, left, left2);

    cv::Point maxP;
    cv::minMaxLoc(cornerMap, nullptr, nullptr, nullptr, &maxP);

    cv::Point2f maxP2;
    maxP2 = findSubpixelEyeCorner(cornerMap, maxP);

    return maxP2;
}

cv::Point2f EyeDetector::findSubpixelEyeCorner(cv::Mat region, cv::Point maxP)
{
    auto sizeRegion = region.size();

    cv::Mat cornerMap(sizeRegion.height * 10, sizeRegion.width * 10, CV_32F);

    cv::resize(region, cornerMap, cornerMap.size(), 0, 0, cv::INTER_CUBIC);

    cv::Point maxP2;
    cv::minMaxLoc(cornerMap, nullptr, nullptr, nullptr, &maxP2);

    return cv::Point2f(static_cast<float>(sizeRegion.width / 2 + maxP2.x / 10),
                       static_cast<float>(sizeRegion.height / 2 + maxP2.y / 10));
}

cv::Point EyeDetector::unscalePoint(cv::Point p, cv::Rect origSize) const
{
    auto ratio = static_cast<float>(kFastEyeWidth) / origSize.width;
    auto x = ROUND_INT(p.x / ratio);
    auto y = ROUND_INT(p.y / ratio);
    return cv::Point(x, y);
}

void EyeDetector::scaleToFastSize(const cv::Mat& src, cv::Mat& dst) const
{
    cv::resize(src, dst, cv::Size(kFastEyeWidth, 
        static_cast<int>(static_cast<float>(kFastEyeWidth) / src.cols * src.rows)));
}

void EyeDetector::testPossibleCentersFormula(int x, int y, unsigned char weight, double gx, double gy, cv::Mat& out)
{
    // for all possible centers
    for (int cy = 0; cy < out.rows; ++cy)
    {
        double* Or = out.ptr<double>(cy);
        for (int cx = 0; cx < out.cols; ++cx)
        {
            if (x == cx && y == cy)
            {
                continue;
            }
            // create a vector from the possible center to the gradient origin
            double dx = x - cx;
            double dy = y - cy;
            // normalize d
            double magnitude = sqrt((dx * dx) + (dy * dy));
            dx = dx / magnitude;
            dy = dy / magnitude;
            double dotProduct = dx * gx + dy * gy;
            dotProduct = std::max(0.0, dotProduct);
            // square and multiply by the weight
            if (kEnableWeight)
            {
                Or[cx] += dotProduct * dotProduct * (weight / kWeightDivisor);
            }
            else
            {
                Or[cx] += dotProduct * dotProduct;
            }
        }
    }
}

bool floodShouldPushPoint(const cv::Point& np, const cv::Mat& mat)
{
    return np.x >= 0 && np.x < mat.cols && np.y >= 0 && np.y < mat.rows;
}

cv::Mat EyeDetector::floodKillEdges(cv::Mat& mat)
{
    rectangle(mat, cv::Rect(0, 0, mat.cols, mat.rows), 255);

    cv::Mat mask(mat.rows, mat.cols, CV_8U, 255);
    std::queue<cv::Point> toDo;
    toDo.push(cv::Point(0, 0));
    while (!toDo.empty())
    {
        cv::Point p = toDo.front();
        toDo.pop();
        if (mat.at<float>(p) == 0.0f)
            continue;

        // add in every direction
        cv::Point np(p.x + 1, p.y); // right
        if (floodShouldPushPoint(np, mat)) toDo.push(np);
        np.x = p.x - 1;
        np.y = p.y; // left
        if (floodShouldPushPoint(np, mat)) toDo.push(np);
        np.x = p.x;
        np.y = p.y + 1; // down
        if (floodShouldPushPoint(np, mat)) toDo.push(np);
        np.x = p.x;
        np.y = p.y - 1; // up
        if (floodShouldPushPoint(np, mat)) toDo.push(np);
        // kill it
        mat.at<float>(p) = 0.0f;
        mask.at<uchar>(p) = 0;
    }
    return mask;
}

cv::Mat EyeDetector::matrixMagnitude(const cv::Mat& matX, const cv::Mat& matY)
{
    cv::Mat magnitude(matX.rows, matX.cols, CV_64F);
    cv::sqrt(matX.mul(matX) + matY.mul(matY), magnitude);
    return magnitude;

    //cv::Mat mags(matX.rows, matX.cols, CV_64F);
    //for (int y = 0; y < matX.rows; ++y)
    //{
    //    const double *Xr = matX.ptr<double>(y), *Yr = matY.ptr<double>(y);
    //    double* Mr = mags.ptr<double>(y);
    //    for (int x = 0; x < matX.cols; ++x)
    //    {
    //        auto gX = Xr[x], gY = Yr[x];
    //        auto magnitude = sqrt((gX * gX) + (gY * gY));
    //        Mr[x] = magnitude;
    //    }
    //}
    //return mags;
}

double EyeDetector::computeDynamicThreshold(const cv::Mat& mat, double stdDevFactor) const
{
    cv::Scalar stdMagnGrad, meanMagnGrad;
    cv::meanStdDev(mat, meanMagnGrad, stdMagnGrad);
    auto stdDev = stdMagnGrad[0] / sqrt(mat.rows * mat.cols);
    return stdDevFactor * stdDev + meanMagnGrad[0];
}
