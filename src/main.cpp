#include <iostream>
#include <cstring>
#include "utilities/OBJLoader.hpp"
#include "utilities/lodepng.h"
#include "rasteriser.hpp"
#include "mpi.h"

float rotationAngle(int world_size, float world_rank);

int main(int argc, char **argv) {

	MPI_Init(NULL, NULL);

	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	int world_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int name_len;
	MPI_Get_processor_name(processor_name, &name_len);

	std::string input("../input/sphere.obj");
	std::string output("../output/sphere.png");
	unsigned int width = 1920;
	unsigned int height = 1080;
	unsigned int depth = 3;

	for (int i = 1; i < argc; i++) {
		if (i < argc -1) {
			if (std::strcmp("-i", argv[i]) == 0) {
				input = argv[i+1];
			} else if (std::strcmp("-o", argv[i]) == 0) {
				output = argv[i+1];
			} else if (std::strcmp("-w", argv[i]) == 0) {
				width = (unsigned int) std::stoul(argv[i+1]);
			} else if (std::strcmp("-h", argv[i]) == 0) {
				height = (unsigned int) std::stoul(argv[i+1]);
			} else if (std::strcmp("-d", argv[i]) == 0) {
				depth = (int) std::stoul(argv[i+1]);
			}
		}
	}

	MPI_Datatype MPI_FLOAT4;
	constexpr int const MPI_FLOAT4_blocklength[4] = {1, 1, 1, 1};
	constexpr MPI_Aint const MPI_FLOAT4_displacement[4] = {
			offsetof(float4, x),
			offsetof(float4, y),
			offsetof(float4, z),
			offsetof(float4, w)
	};
	constexpr MPI_Datatype const MPI_FLOAT4_types[4] = {
			MPI_FLOAT,
			MPI_FLOAT,
			MPI_FLOAT,
			MPI_FLOAT
	};
	MPI_Type_create_struct(
			4,
			MPI_FLOAT4_blocklength,
			MPI_FLOAT4_displacement,
			MPI_FLOAT4_types,
			&MPI_FLOAT4);
	MPI_Type_commit(&MPI_FLOAT4);


	MPI_Datatype MPI_FLOAT3;
	constexpr int const MPI_FLOAT3_blocklength[3] = {1, 1, 1};
	constexpr MPI_Aint const MPI_FLOAT3_displacement[3] = {
			offsetof(float3, x),
			offsetof(float3, y),
			offsetof(float3, z)
	};
	constexpr MPI_Datatype const MPI_FLOAT3_types[3] = {
			MPI_FLOAT,
			MPI_FLOAT,
			MPI_FLOAT
	};
	MPI_Type_create_struct(
			3,
			MPI_FLOAT3_blocklength,
			MPI_FLOAT3_displacement,
			MPI_FLOAT3_types,
			&MPI_FLOAT3);
	MPI_Type_commit(&MPI_FLOAT3);



	std::cout << "Loading '" << input << "' file... " << std::endl;
	std::vector<Mesh> meshs;

	meshs = loadWavefront(input, false);
	int vertSize;
	int normalsSize;
	int texturesSize;

	if(world_rank != 0){
		for(unsigned int i = 0; i < meshs.size(); ++i){

			vertSize = meshs.at(i).vertices.size();
			normalsSize = meshs.at(i).normals.size();
			texturesSize = meshs.at(i).textures.size();

			meshs.at(i).vertices.clear();
			meshs.at(i).normals.clear();
			meshs.at(i).textures.clear();

			meshs.at(i).vertices.resize(vertSize);
			meshs.at(i).normals.resize(normalsSize);
			meshs.at(i).textures.resize(texturesSize);
		}
	}

	for(unsigned int i = 0; i < meshs.size(); ++i) {
		MPI_Bcast(&meshs.at(i).vertices.at(0), meshs.at(i).vertices.size() , MPI_FLOAT4, 0, MPI_COMM_WORLD);
		MPI_Bcast(&meshs.at(i).normals.at(0), meshs.at(i).normals.size() , MPI_FLOAT3, 0, MPI_COMM_WORLD);
		MPI_Bcast(&meshs.at(i).textures.at(0), meshs.at(i).textures.size() , MPI_FLOAT3, 0, MPI_COMM_WORLD);
	}

	std::vector<unsigned char> frameBuffer = rasterise(1, meshs, width, height, depth);

	//append a number to the ending of the file name.
	output = output.substr(0, output.find(".", 0)) + std::to_string(world_rank) + ".png";

	std::cout << "Writing image to '" << output << "'..." << std::endl;

	unsigned error = lodepng::encode(output, frameBuffer, width, height);

	if(error)
	{
		std::cout << "An error occurred while writing the image file: " << error << ": " << lodepng_error_text(error) << std::endl;
	}
	MPI_Finalize();
}

float rotationAngle(int world_size, float world_rank){
    return 360/world_size*world_rank;
}
