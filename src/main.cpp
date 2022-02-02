#include <cxxopts/cxxopts.hpp>

#include <iostream>
#include <string>
#include <vector>

#include "./loaders/ObjLoader.h"
#include "./exporters/ObjExporter.h"
#include "./exporters/GLTFExporter.h"
#include "./exporters/B3DMExporter.h"
#include "./split/SplitBase.h"
#include "./split/VoxelsSplitter.h"
#include "./split/RegularSplitter.h"
#include "./utils.h"

#include "./tiles/Tileset.h"

#include <GLTF/GLTFScene.h>
#include <GLTF/GLTFMesh.h>
#include <GLTF/GLTFMaterial.h>
#include <GLTF/GLTFPrimitive.h>
#include <GLTF/GLTFNode.h>
#include <GLTF/GLTFBuffer.h>
#include <GLTF/GLTFBufferView.h>
#include <GLTF/GLTFAccessor.h>
#include <GLTF/GLTFAsset.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <stb/stb_image_resize.h>


int main(int argc, char** argv) {
  cxxopts::Options options("3dTG", "3d models compiler from various formats to tile format.");

  options.add_options()
  ("i,input", "Input model path", cxxopts::value<std::string>())
  ("o,output", "Output directory", cxxopts::value<std::string>()->default_value("./exported"))
  ("l,limit", "Polygons per chunk limit", cxxopts::value<unsigned int>()->default_value("2048"))
  ("g,grid", "Grid resolution", cxxopts::value<unsigned int>()->default_value("64"))
  ("iso", "Iso level", cxxopts::value<float>()->default_value("1.0"))
  ("f,format", "Model format to export", cxxopts::value<std::string>()->default_value("b3dm"))
  ("a,algorithm", "Algorithm to use to split", cxxopts::value<std::string>()->default_value("voxel"))
  ("h,help", "Help")
  ;

  options.allow_unrecognised_options();

  cxxopts::ParseResult result;

  try {
    result = options.parse(argc, argv);
  } catch (cxxopts::missing_argument_exception const &exeption) {
    std::cout << exeption.what() << std::endl;
    return 0;
  }

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  if (!result.count("input")) {
    std::cout << "Input isn't specified" << std::endl;
    exit(0);
  }

  std::string algorithm = result["algorithm"].as<std::string>();
  
  std::string voxelAl = std::string("voxel");
  std::string regularAl = std::string("regular");

  if (algorithm != voxelAl && algorithm != regularAl) {
    std::cout << "Algorithm \"" << algorithm << "\" wasn't found, available algoritms: " << voxelAl << ", " << regularAl << std::endl;
    exit(0);
  }

  std::string inputFile = utils::normalize(result["input"].as<std::string>());
  std::cout << "Importing " << inputFile.c_str() << std::endl;

  ObjLoader loader;
  loader.parse(inputFile.c_str()); 

  std::cout << "Import finished" << std::endl;



  std::string out = utils::normalize(result["output"].as<std::string>());

  /*
  if (argc < 3) { 
      out = utils::concatPath(utils::getDirectory(inputFile), "exported");
  } else {
      out = utils::getDirectory(argv[2]);
  }
  */

  std::cout << "Output directory: " << out.c_str() << std::endl;

  GLTFExporter exporter;


  std::cout << "Splitting..." << std::endl;
  

  float totalError = 0.0f;
  Tileset tileset(0);

  std::shared_ptr<SplitInterface> splitInstance;

  if (algorithm == voxelAl) {
    std::shared_ptr<VoxelsSplitter> voxelSplit = std::make_shared<VoxelsSplitter>();
    voxelSplit->polygonsLimit = result["limit"].as<unsigned int>();

    unsigned int gr = result["grid"].as<unsigned int>();
    voxelSplit->gridSettings.gridResolution = glm::ivec3(gr, gr, gr);

    voxelSplit->gridSettings.isoLevel = result["iso"].as<float>();

    splitInstance = voxelSplit;
  } else {
    std::shared_ptr<RegularSplitter> regularSplit = std::make_shared<RegularSplitter>();
    regularSplit->polygonLimit = result["limit"].as<unsigned int>();

    splitInstance = regularSplit;
  }

  
  unsigned int chunk = 0;
  unsigned int processed = 0;

  splitInstance->onSave = [&](GroupObject object, IdGenerator::ID targetId, IdGenerator::ID parentId, unsigned int level, bool indexedGeometry){
      object->computeBoundingBox();
      object->computeGeometricError();

      std::string modelDir = std::string("level_") + std::to_string(level);
      std::string modelName = utils::getFileName(object->name) + "_" + std::to_string(chunk);
      std::string modelPath = utils::normalize(
          utils::concatPath("./", utils::concatPath(modelDir, modelName)) + std::string(".") + exporter.format
      );

      std::shared_ptr<Tile> parentTile = tileset.findTileById(parentId);
      if (parentTile != NULL) {
          std::shared_ptr<Tile> targetTile = std::make_shared<Tile>();
          targetTile->id = targetId;
          targetTile->geometricError = object->geometricError;
          //   std::cout << "Geom error: " << targetTile->geometricError << std::endl;
          // targetTile->refine = TileRefine::REPLASE;

          totalError += (float) object->geometricError;

          targetTile->content = std::make_shared<TileContent>();
          targetTile->content->uri = modelPath;

          targetTile->boundingVolume = std::make_shared<TileBoundingVolume>();
          targetTile->boundingVolume->box = std::make_shared<TileBoundingBox>();

          targetTile->boundingVolume->box->center = object->boundingBox.getCenter();
          targetTile->boundingVolume->box->xHalf = object->boundingBox.getSize();
          targetTile->boundingVolume->box->xHalf /= 2.0;

          targetTile->boundingVolume->box->yHalf = targetTile->boundingVolume->box->xHalf;
          targetTile->boundingVolume->box->zHalf = targetTile->boundingVolume->box->xHalf;

          targetTile->boundingVolume->box->xHalf.y = 0.0f;
          targetTile->boundingVolume->box->xHalf.z = 0.0f;

          targetTile->boundingVolume->box->yHalf.x = 0.0f;
          targetTile->boundingVolume->box->yHalf.z = 0.0f;

          targetTile->boundingVolume->box->zHalf.x = 0.0f;
          targetTile->boundingVolume->box->zHalf.y = 0.0f;

          parentTile->children.push_back(targetTile);
      }
      
      exporter.save(utils::concatPath(out, modelDir), modelName, object, indexedGeometry);

      chunk++;

      processed++;
      // std::cout << "Splitting model " << (processed + 1) << std::endl;
  };

  splitInstance->split(loader.object);
  splitInstance->finish();
  
  
  std::cout << "Exported" << std::endl;

  std::cout << "Saving JSON" << std::endl;
  // tileset.computeRootGeometricError();
  tileset.setRootGeometricError(totalError);
  tileset.computeRootBoundingVolume();

  std::fstream fs;
  fs.open(utils::concatPath(out, "tileset.json"), std::fstream::out);
  
  fs << tileset.toJSON().dump(2);

  fs.close();
  std::cout << "Saved" << std::endl;

  loader.free();
  
  return 0;
}