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

#include "../Orthanc/Core/WebServiceParameters.h"

#include <list>
#include <string>
#include <boost/thread/mutex.hpp>
#include <json/value.h>

namespace OrthancPlugins
{
  class DicomWebPeers
  {
  private:
    typedef std::map<std::string, Orthanc::WebServiceParameters*>  Peers;

    boost::mutex  mutex_;
    Peers         peers_;

    void Clear();

    DicomWebPeers()  // Forbidden (singleton pattern)
    {
    }

  public:
    void Load(const Json::Value& configuration);

    ~DicomWebPeers()
    {
      Clear();
    }

    static DicomWebPeers& GetInstance();

    Orthanc::WebServiceParameters GetPeer(const std::string& name);

    void ListPeers(std::list<std::string>& peers);
  };
}
