// GenerateVirtualDataPerfusionV1.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocGenerateVirtualDataPerfusionV1();

Drover GenerateVirtualDataPerfusionV1(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                      const std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/);
