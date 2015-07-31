/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
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

#include <orthanc/OrthancCPlugin.h>
#include <json/value.h>

namespace OrthancPlugins
{
  namespace Configuration
  {
    bool Read(Json::Value& configuration,
              OrthancPluginContext* context);

    std::string GetStringValue(const Json::Value& configuration,
                               const std::string& key,
                               const std::string& defaultValue);
    
    bool GetBoolValue(const Json::Value& configuration,
                      const std::string& key,
                      bool defaultValue);

    std::string GetRoot(const Json::Value& configuration);

    std::string GetBaseUrl(const Json::Value& configuration,
                           const OrthancPluginHttpRequest* request);
  }
}
