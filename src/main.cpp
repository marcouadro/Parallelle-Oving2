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

	typedef struct vectors {
		std::vector<float4> vertices;
		std::vector<float3> textures;
		std::vector<float3> normals;
	} vectors;

	MPI_Datatype MPI_vectors;
	constexpr int const mpi_vectors_blocklength[3] = {216, 216, 216};
	constexpr MPI_Aint const mpi_vectors_displacement[3] = {
			offsetof(vectors, vertices),
			offsetof(vectors, textures),
			offsetof(vectors, normals)
	};
	constexpr MPI_Datatype const mpi_vectors_types[3] = {
			MPI_FLOAT,
			MPI_FLOAT,
			MPI_FLOAT
	};
	MPI_Type_create_struct(
			3,
			mpi_vectors_blocklength,
			mpi_vectors_displacement,
			mpi_vectors_types,
			&MPI_vectors);
	MPI_Type_commit(&MPI_vectors);


	std::cout << "Loading '" << input << "' file... " << std::endl;
	std::vector<Mesh> meshs;

	vectors val;
	int size;
    if (world_rank == 0) { //Master
        meshs = loadWavefront(input, false);
        for(unsigned int i = 0; i < meshs.size(); ++i){
           for(unsigned int j = 0; j < meshs.at(i).vertices.size(); ++j){
               val.vertices.emplace_back(meshs.at(i).vertices.at(j)); //Flatten array
               val.textures.emplace_back(meshs.at(i).textures.at(j));
               val.normals.emplace_back(meshs.at(i).normals.at(j));

               /*val.vertices.at(i) = meshs.at(i).vertices.at(j);
               val.textures.at(i) = meshs.at(i).textures.at(j);
               val.normals.at(i) = meshs.at(i).normals.at(j);*/
           }
        }
        size = (int) val.vertices.size()*3;
    }
    //Synchronize
    MPI_Barrier(MPI_COMM_WORLD);
    std::cout << "SIZE of vert: " << size << std::endl;
    MPI_Bcast(&val, size, MPI_vectors, 0, MPI_COMM_WORLD);
    std::cout << "SIZE AFTER BCAST: " << size << std::endl;

    meshs = loadWavefront(input, false); //Each rank loads the mesh file.

   // unsigned int size = (int) meshs.at(0).vertices.size()/meshs.size();


    unsigned int indexCounter = 0;

    for(unsigned int i = 0; i < val.vertices.size(); ++i) {
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