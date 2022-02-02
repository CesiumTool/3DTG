#ifndef __OBJEXPORTER_H__
#define __OBJEXPORTER_H__

#include "./Exporter.h"


class ObjExporter : public Exporter {
  public:
    void save(std::string directory, std::string fileName, GroupObject object, bool indexedGeometry);
    void saveMaterial(std::string directory, std::string fileName, MaterialMap materialMap);
    std::string format = "obj";
};

#endif