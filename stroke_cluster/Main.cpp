#include <iostream>
#include <fstream>
#include <sstream>
#include <numeric>
#include <limits>

#include <algorithm>
#include <iostream>

#include "StrokeCutting.h"
#include "StrokeOrientation.h"
#include "Cluster.h"
#include "Parameterization.h"

void to_scap(std::string const &filename, Capture const &capture, int width, int height) {
	std::ofstream scap_ofs(filename);
	std::string out_buffer;
	out_buffer += capture.to_string();

	scap_ofs << "#" << width << "\t" << height << std::endl;
	scap_ofs.write(out_buffer.c_str(), out_buffer.size());
	scap_ofs.close();
}

Input from_capture(Capture capture) {
	Input input;
	auto& clusters = input.clusters;

	input.thickness = capture.thickness;

	double min_x = std::numeric_limits<double>::infinity();
	double max_x = -std::numeric_limits<double>::infinity();
	double min_y = std::numeric_limits<double>::infinity();
	double max_y = -std::numeric_limits<double>::infinity();

	for (auto& polyline : capture.sketchedPolylines) {
		polyline.reparameterize(3 * capture.thickness);

		for (auto& point : polyline.points) {
			min_x = std::min(min_x, point.first.x);
			max_x = std::max(max_x, point.first.x);
			min_y = std::min(min_y, point.first.y);
			max_y = std::max(max_y, point.first.y);
		}
	}
	glm::dvec2 center((max_x + min_x) / 2, (max_y + min_y) / 2);
	input.width = (max_x - min_x) / capture.thickness;
	input.height = (max_y - min_y) / capture.thickness;

	for (auto& polyline : capture.sketchedPolylines) {
		if (polyline.points.empty()) continue;
		clusters[polyline.group_ind].strokes.emplace_back();
		auto& stroke = clusters[polyline.group_ind].strokes.back();
		stroke.points.reserve(polyline.points.size());
		stroke.u.reserve(polyline.points.size());
		for (auto& point : polyline.points) {
			// Recenter and normalize to stroke width
			stroke.points.push_back((glm::dvec2(point.first.x, point.first.y) - center) / capture.thickness);
			stroke.u.push_back(0);
		}
	}

	return input;
}

int main(int argc, char** argv) {
	std::string scap_filename(argv[argc - 1]);
	//std::string scap_filename = "D:\\strokestrip\\cmd_SA_cluster.scap";

	Input input;

	// 1. Preprocess
	{
		std::ifstream scap_ifs(scap_filename);
		std::stringstream buffer;
		buffer << scap_ifs.rdbuf();
		scap_ifs.close();

		Capture capture;
		capture.from_string(buffer.str());

		std::string canvas_size_line = buffer.str().substr(0, buffer.str().find_first_of("\n"));
		int width;
		int height;
		sscanf(canvas_size_line.c_str(), "#%d\t%d", &width, &height);

		// Assign missing ids
		int max_ind = -1;
		for (size_t i = 0; i < capture.sketchedPolylines.size(); i++) {
			max_ind = std::max(max_ind, capture.getSketchedPolyline(i).stroke_ind);
		}

		preprocess_cluster(1, width, height, &capture);
		input = from_capture(capture);
	}

	// 2. Orientation
	{
		bool debug_orientation = true;
		StrokeOrientation orientation(debug_orientation);
		orientation.orient_strokes(input);
		if (debug_orientation) {
			std::string final_output_name = scap_filename;
			final_output_name.erase(final_output_name.length() - 5, 5); // remove .scap
			final_output_name += "_orientation_debug.svg";
			std::ofstream orientation_svg(final_output_name);
			orientation.orientation_debug(orientation_svg, input);
		}
		orientation.flip_strokes(&input);
	}

	// 3. Parameterization
	{
		bool debug_parameterization = true;
		Parameterization param(debug_parameterization);
		param.parameterize(&input);
		if (debug_parameterization) {
			std::string final_output_name = scap_filename;
			final_output_name.erase(final_output_name.length() - 5, 5); // remove .scap
			final_output_name += "_param_debug.svg";
			std::ofstream debug_svg(final_output_name);
			param.debug_svg(debug_svg, input);
		}
		{
			std::string final_output_name = scap_filename;
			final_output_name.erase(final_output_name.length() - 5, 5); // remove .scap
			final_output_name += "_isolines.svg";
			std::ofstream isolines_svg(final_output_name);
			param.isolines_svg(isolines_svg, input);
		}
	}

	{
		std::string final_output_name = scap_filename;
		final_output_name.erase(final_output_name.length() - 5, 5); // remove .scap
		final_output_name += "_rainbow.svg";
		std::ofstream param_svg(final_output_name);
		input.param_svg(param_svg, true);
	}

	{
		std::string final_output_name = scap_filename;
		final_output_name.erase(final_output_name.length() - 5, 5); // remove .scap
		final_output_name += "_orientation.svg";
		std::ofstream orientation_svg(final_output_name);
		input.orientation_svg(orientation_svg);
	}

	/*{
		Capture result = to_capture(input);
		to_scap(final_output_name, result, input.width * input.thickness, input.height * input.thickness);
	}*/

	return 0;
}