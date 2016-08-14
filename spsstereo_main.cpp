/*
    Copyright (C) 2014  Koichiro Yamaguchi

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include "SPSStereo.h"
#include "defParameter.h"


void makeSegmentBoundaryImage(const cv::Mat & inputImage,
							  const /*png::image<png::gray_pixel_16>*/ cv::Mat & segmentImage,
							  std::vector< std::vector<int> >& boundaryLabels,
							  cv::Mat& segmentBoundaryImage);
void writeDisparityPlaneFile(const std::vector< std::vector<double> >& disparityPlaneParameters, const std::string outputDisparityPlaneFilename);
void writeBoundaryLabelFile(const std::vector< std::vector<int> >& boundaryLabels, const std::string outputBoundaryLabelFilename);


int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "usage: sgmstereo left right" << std::endl;
		exit(1);
	}
    
    std::string leftImageFilename;
    std::string rightImageFilename;

    std::ifstream images(argv[1]);
    
    std::getline(images, leftImageFilename);
    std::getline(images, rightImageFilename);
    while (!images.eof())
    {
        std::cout << "Procesing : " << leftImageFilename << std::endl;
        cv::Mat leftImage = cv::imread(leftImageFilename, CV_LOAD_IMAGE_COLOR);
        cv::Mat rightImage = cv::imread(rightImageFilename, CV_LOAD_IMAGE_COLOR);

        SPSStereo sps;
        sps.setIterationTotal(outerIterationTotal, innerIterationTotal);
        sps.setWeightParameter(lambda_pos, lambda_depth, lambda_bou, lambda_smo);
        sps.setInlierThreshold(lambda_d);
        sps.setPenaltyParameter(lambda_hinge, lambda_occ, lambda_pen);

        cv::Mat segmentImage;
        cv::Mat disparityImage;
        std::vector< std::vector<double> > disparityPlaneParameters;
        std::vector< std::vector<int> > boundaryLabels;
        sps.compute(superpixelTotal, leftImage, rightImage, segmentImage, disparityImage, disparityPlaneParameters, boundaryLabels);

        cv::Mat segmentBoundaryImage;
        makeSegmentBoundaryImage(leftImage, segmentImage, boundaryLabels, segmentBoundaryImage);

        std::string outputBaseFilename = leftImageFilename;
        size_t slashPosition = outputBaseFilename.rfind('/');
        if (slashPosition != std::string::npos) outputBaseFilename.erase(0, slashPosition + 1);
        size_t dotPosition = outputBaseFilename.rfind('.');
        if (dotPosition != std::string::npos) outputBaseFilename.erase(dotPosition);
        std::string outputDisparityImageFilename = outputBaseFilename + "_left_disparity.png";
        std::string outputSegmentImageFilename = outputBaseFilename + "_segment.png";
        std::string outputBoundaryImageFilename = outputBaseFilename + "_boundary.png";
        std::string outputDisparityPlaneFilename = outputBaseFilename + "_plane.txt";
        std::string outputBoundaryLabelFilename = outputBaseFilename + "_label.txt";

        cv::imwrite(outputDisparityImageFilename, disparityImage);
        cv::imwrite(outputSegmentImageFilename, segmentImage);
        cv::imwrite(outputBoundaryImageFilename, segmentBoundaryImage);
        writeDisparityPlaneFile(disparityPlaneParameters, outputDisparityPlaneFilename);
        writeBoundaryLabelFile(boundaryLabels, outputBoundaryLabelFilename);
        std::getline(images, leftImageFilename);
        std::getline(images, rightImageFilename);
    }
    return 0;
}


void makeSegmentBoundaryImage(const cv::Mat & inputImage,
							  const /*png::image<png::gray_pixel_16>*/ cv::Mat & segmentImage,
							  std::vector< std::vector<int> >& boundaryLabels,
							  cv::Mat& segmentBoundaryImage)
{
	int width = static_cast<int>(inputImage.cols);
	int height = static_cast<int>(inputImage.rows);
	int boundaryTotal = static_cast<int>(boundaryLabels.size());

	segmentBoundaryImage.create(height, width, CV_8UC3);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			segmentBoundaryImage.at<cv::Vec3b>(y,x) = inputImage.at<cv::Vec3b>(y, x);
		}
	}

	int boundaryWidth = 2;
	for (int y = 0; y < height - 1; ++y) {
		for (int x = 0; x < width - 1; ++x) {
			int pixelLabelIndex = segmentImage.at<uint16_t>(y, x);

			if (segmentImage.at<uint16_t>(y, x + 1) != pixelLabelIndex) {
				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (x - w >= 0) segmentBoundaryImage.at<cv::Vec3b>(y, x - w) = cv::Vec3b(128, 128, 128);
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (x + w < width) segmentBoundaryImage.at<cv::Vec3b>(y, x + w) =  cv::Vec3b(128, 128, 128);
				}
			}
			if (segmentImage.at<uint16_t>( y + 1, x) != pixelLabelIndex) {
				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (y - w >= 0) segmentBoundaryImage.at<cv::Vec3b>(y - w, x) =  cv::Vec3b(128, 128, 128);
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (y + w < height) segmentBoundaryImage.at<cv::Vec3b>(y + w, x) = cv::Vec3b(128, 128, 128);
				}
			}
		}
	}

	boundaryWidth = 7;
	for (int y = 0; y < height - 1; ++y) {
		for (int x = 0; x < width - 1; ++x) {
			int pixelLabelIndex = segmentImage.at<uint16_t>(y, x);

			if (segmentImage.at<uint16_t>(y, x + 1) != pixelLabelIndex) {
				cv::Vec3b negativeSideColor, positiveSideColor;
				int pixelBoundaryIndex = -1;
				for (int boundaryIndex = 0; boundaryIndex < boundaryTotal; ++boundaryIndex) {
					if ((boundaryLabels[boundaryIndex][0] == pixelLabelIndex && boundaryLabels[boundaryIndex][1] == segmentImage.at<uint16_t>(y, x + 1))
						|| (boundaryLabels[boundaryIndex][0] == segmentImage.at<uint16_t>(y, x + 1) && boundaryLabels[boundaryIndex][1] == pixelLabelIndex))
					{
						pixelBoundaryIndex = boundaryIndex;
						break;
					}
				}
				if (boundaryLabels[pixelBoundaryIndex][2] == 3) continue;
				else if (boundaryLabels[pixelBoundaryIndex][2] == 2) {
					negativeSideColor.val[2] = 0;  negativeSideColor.val[1] = 225;  negativeSideColor.val[0] = 0;
					positiveSideColor.val[2] = 0;  positiveSideColor.val[1] = 225;  positiveSideColor.val[0] = 0;
				} else if (pixelLabelIndex == boundaryLabels[pixelBoundaryIndex][boundaryLabels[pixelBoundaryIndex][2]]) {
					negativeSideColor.val[2] = 225;  negativeSideColor.val[1] = 0;  negativeSideColor.val[0] = 0;
					positiveSideColor.val[2] = 0;  positiveSideColor.val[1] = 0;  positiveSideColor.val[0] = 225;
				} else {
					negativeSideColor.val[2] = 0;  negativeSideColor.val[1] = 0;  negativeSideColor.val[0] = 225;
					positiveSideColor.val[2] = 225;  positiveSideColor.val[1] = 0;  positiveSideColor.val[0] = 0;
				}

				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (x - w >= 0) segmentBoundaryImage.at<cv::Vec3b>(y, x - w) = negativeSideColor;
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (x + w < width) segmentBoundaryImage.at<cv::Vec3b>(y, x + w) = positiveSideColor;
				}
			}
			if (segmentImage.at<uint16_t>(y + 1, x) != pixelLabelIndex) {
                cv::Vec3b negativeSideColor, positiveSideColor;
				int pixelBoundaryIndex = -1;
				for (int boundaryIndex = 0; boundaryIndex < boundaryTotal; ++boundaryIndex) {
					if ((boundaryLabels[boundaryIndex][0] == pixelLabelIndex && boundaryLabels[boundaryIndex][1] == segmentImage.at<uint16_t>(y + 1, x))
						|| (boundaryLabels[boundaryIndex][0] == segmentImage.at<uint16_t>(y + 1, x) && boundaryLabels[boundaryIndex][1] == pixelLabelIndex))
					{
						pixelBoundaryIndex = boundaryIndex;
						break;
					}
				}
				if (boundaryLabels[pixelBoundaryIndex][2] == 3) continue;
				else if (boundaryLabels[pixelBoundaryIndex][2] == 2) {
					negativeSideColor.val[2] = 0;  negativeSideColor.val[1] = 225;  negativeSideColor.val[0] = 0;
					positiveSideColor.val[2] = 0;  positiveSideColor.val[1] = 225;  positiveSideColor.val[0] = 0;
				} else if (pixelLabelIndex == boundaryLabels[pixelBoundaryIndex][boundaryLabels[pixelBoundaryIndex][2]]) {
					negativeSideColor.val[2] = 225;  negativeSideColor.val[1] = 0;  negativeSideColor.val[0] = 0;
					positiveSideColor.val[2] = 0;  positiveSideColor.val[1] = 0;  positiveSideColor.val[0] = 225;
				} else {
					negativeSideColor.val[2] = 0;  negativeSideColor.val[1] = 0;  negativeSideColor.val[0] = 225;
					positiveSideColor.val[2] = 225;  positiveSideColor.val[1] = 0;  positiveSideColor.val[0] = 0;
				}

				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (y - w >= 0) segmentBoundaryImage.at<cv::Vec3b>(y - w, x) = negativeSideColor;
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (y+ w < height) segmentBoundaryImage.at<cv::Vec3b>(y + w, x) = positiveSideColor;
				}
			}
		}
	}
}

void writeDisparityPlaneFile(const std::vector< std::vector<double> >& disparityPlaneParameters, const std::string outputDisparityPlaneFilename) {
	std::ofstream outputFileStream(outputDisparityPlaneFilename.c_str(), std::ios_base::out);
	if (outputFileStream.fail()) {
		std::cerr << "error: can't open file (" << outputDisparityPlaneFilename << ")" << std::endl;
		exit(0);
	}

	int segmentTotal = static_cast<int>(disparityPlaneParameters.size());
	for (int segmentIndex = 0; segmentIndex < segmentTotal; ++segmentIndex) {
		outputFileStream << disparityPlaneParameters[segmentIndex][0] << " ";
		outputFileStream << disparityPlaneParameters[segmentIndex][1] << " ";
		outputFileStream << disparityPlaneParameters[segmentIndex][2] << std::endl;
	}

	outputFileStream.close();
}

void writeBoundaryLabelFile(const std::vector< std::vector<int> >& boundaryLabels, const std::string outputBoundaryLabelFilename) {
	std::ofstream outputFileStream(outputBoundaryLabelFilename.c_str(), std::ios_base::out);
	if (outputFileStream.fail()) {
		std::cerr << "error: can't open output file (" << outputBoundaryLabelFilename << ")" << std::endl;
		exit(1);
	}

	int boundaryTotal = static_cast<int>(boundaryLabels.size());
	for (int boundaryIndex = 0; boundaryIndex < boundaryTotal; ++boundaryIndex) {
		outputFileStream << boundaryLabels[boundaryIndex][0] << " ";
		outputFileStream << boundaryLabels[boundaryIndex][1] << " ";
		outputFileStream << boundaryLabels[boundaryIndex][2] << std::endl;
	}
	outputFileStream.close();
}
