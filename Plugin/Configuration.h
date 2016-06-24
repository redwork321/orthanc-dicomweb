/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "../Orthanc/Core/Enumerations.h"

#include <orthanc/OrthancCPlugin.h>
#include <json/value.h>

#if (ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER <= 0 && \
     ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER <= 9 && \
     ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER <= 6)
#  define HAS_SEND_MULTIPART_ITEM_2   0
#else
#  define HAS_SEND_MULTIPART_ITEM_2   1
#endif

namespace OrthancPlugins
{
  struct MultipartItem
  {
    const char*   data_;
    size_t        size_;
    std::string   contentType_;
  };

  bool LookupHttpHeader(std::string& value,
                        const OrthancPluginHttpRequest* request,
                        const std::string& header);

  void ParseContentType(std::string& application,
                        std::map<std::string, std::string>& attributes,
                        const std::string& header);

  void ParseMultipartBody(std::vector<MultipartItem>& result,
                          OrthancPluginContext* context,
                          const char* body,
                          const uint64_t bodySize,
                          const std::string& boundary);

  void ParseAssociativeArray(std::map<std::string, std::string>& target,
                             const Json::Value& value,
                             const std::string& key);

  namespace Configuration
  {
    void Initialize(OrthancPluginContext* context);

    OrthancPluginContext* GetContext();
    
    std::string GetStringValue(const std::string& key,
                               const std::string& defaultValue);

    bool GetBooleanValue(const std::string& key,
                         bool defaultValue);

    unsigned int GetUnsignedIntegerValue(const std::string& key,
                                         unsigned int defaultValue);

    std::string GetRoot();

    std::string GetWadoRoot();
      
    std::string GetBaseUrl(const OrthancPluginHttpRequest* request);

    std::string GetWadoUrl(const std::string& wadoBase,
                           const std::string& studyInstanceUid,
                           const std::string& seriesInstanceUid,
                           const std::string& sopInstanceUid);

    void LogError(const std::string& message);

    void LogWarning(const std::string& message);

    void LogInfo(const std::string& message);

    Orthanc::Encoding GetDefaultEncoding();
  }
}
