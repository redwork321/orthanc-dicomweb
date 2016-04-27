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


#include "Plugin.h"

#include "QidoRs.h"
#include "StowRs.h"
#include "StowRsClient.h"
#include "WadoRs.h"
#include "WadoUri.h"
#include "Configuration.h"
#include "DicomWebPeers.h"

#include <gdcmDictEntry.h>
#include <gdcmDict.h>
#include <gdcmDicts.h>
#include <gdcmGlobal.h>


// Global state
OrthancPluginContext* context_ = NULL;
Json::Value configuration_;
const gdcm::Dict* dictionary_ = NULL;

#include "../Orthanc/Core/OrthancException.h"
#include <boost/lexical_cast.hpp>


typedef void (*RestCallback) (OrthancPluginRestOutput* output,
                              const char* url,
                              const OrthancPluginHttpRequest* request);


template <RestCallback Callback>
OrthancPluginErrorCode Protect(OrthancPluginRestOutput* output,
                               const char* url,
                               const OrthancPluginHttpRequest* request)
{
  try
  {
    Callback(output, url, request);
    return OrthancPluginErrorCode_Success;
  }
  catch (Orthanc::OrthancException& e)
  {
    OrthancPluginLogError(context_, e.What());
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (boost::bad_lexical_cast& e)
  {
    OrthancPluginLogError(context_, e.what());
    return OrthancPluginErrorCode_Plugin;
  }
  catch (std::runtime_error& e)
  {
    OrthancPluginLogError(context_, e.what());
    return OrthancPluginErrorCode_Plugin;
  }
}



void SwitchStudies(OrthancPluginRestOutput* output,
                   const char* url,
                   const OrthancPluginHttpRequest* request)
{
  switch (request->method)
  {
    case OrthancPluginHttpMethod_Get:
      // This is QIDO-RS
      SearchForStudies(output, url, request);
      break;

    case OrthancPluginHttpMethod_Post:
      // This is STOW-RS
      StowCallback(output, url, request);
      break;

    default:
      OrthancPluginSendMethodNotAllowed(context_, output, "GET,POST");
      break;
  }
}


void SwitchStudy(OrthancPluginRestOutput* output,
                 const char* url,
                 const OrthancPluginHttpRequest* request)
{
  switch (request->method)
  {
    case OrthancPluginHttpMethod_Get:
      // This is WADO-RS
      RetrieveDicomStudy(output, url, request);
      break;

    case OrthancPluginHttpMethod_Post:
      // This is STOW-RS
      StowCallback(output, url, request);
      break;

    default:
      OrthancPluginSendMethodNotAllowed(context_, output, "GET,POST");
      break;
  }
}


static void Register(const std::string& root,
                     const std::string& uri,
                     OrthancPluginRestCallback callback)
{
  assert(!uri.empty() && uri[0] != '/');
  std::string s = root + uri;
  OrthancPluginRegisterRestCallback(context_, s.c_str(), callback);
}



void ListPeers(OrthancPluginRestOutput* output,
               const char* url,
               const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
  }
  else
  {
    std::list<std::string> peers;
    OrthancPlugins::DicomWebPeers::GetInstance().ListPeers(peers);

    Json::Value json = Json::arrayValue;
    for (std::list<std::string>::const_iterator it = peers.begin(); it != peers.end(); ++it)
    {
      json.append(*it);
    }

    std::string answer = json.toStyledString(); 
    OrthancPluginAnswerBuffer(context_, output, answer.c_str(), answer.size(), "application/json");
  }
}


void ListPeerOperations(OrthancPluginRestOutput* output,
                        const char* url,
                        const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context_, output, "GET");
  }
  else
  {
    // Make sure the peer does exist
    OrthancPlugins::DicomWebPeers::GetInstance().GetPeer(request->groups[0]);

    Json::Value json = Json::arrayValue;
    json.append("stow");

    std::string answer = json.toStyledString(); 
    OrthancPluginAnswerBuffer(context_, output, answer.c_str(), answer.size(), "application/json");
  }
}



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    context_ = context;

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context_) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context_->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    OrthancPluginSetDescription(context_, "Implementation of DICOMweb (QIDO-RS, STOW-RS and WADO-RS) and WADO-URI.");

    // Read the configuration
    dictionary_ = &gdcm::Global::GetInstance().GetDicts().GetPublicDict();

    configuration_ = Json::objectValue;

    {
      Json::Value tmp;
      if (!OrthancPlugins::Configuration::Read(tmp, context) ||
          tmp.type() != Json::objectValue)
      {
        OrthancPluginLogError(context_, "Unable to read the configuration file");
        return -1;
      }

      if (tmp.isMember("DicomWeb") &&
          tmp["DicomWeb"].type() == Json::objectValue)
      {
        configuration_ = tmp["DicomWeb"];
      }
    }

    OrthancPlugins::DicomWebPeers::GetInstance().Load(configuration_);


    // Configure the DICOMweb callbacks
    if (OrthancPlugins::Configuration::GetBoolValue(configuration_, "Enable", true))
    {
      std::string root = OrthancPlugins::Configuration::GetRoot(configuration_);

      std::string message = "URI to the DICOMweb REST API: " + root;
      OrthancPluginLogWarning(context_, message.c_str());

      Register(root, "instances", Protect<SearchForInstances>);
      Register(root, "series", Protect<SearchForSeries>);    
      Register(root, "studies", Protect<SwitchStudies>);
      Register(root, "studies/([^/]*)", Protect<SwitchStudy>);
      Register(root, "studies/([^/]*)/instances", Protect<SearchForInstances>);    
      Register(root, "studies/([^/]*)/metadata", Protect<RetrieveStudyMetadata>);
      Register(root, "studies/([^/]*)/series", Protect<SearchForSeries>);    
      Register(root, "studies/([^/]*)/series/([^/]*)", Protect<RetrieveDicomSeries>);
      Register(root, "studies/([^/]*)/series/([^/]*)/instances", Protect<SearchForInstances>);    
      Register(root, "studies/([^/]*)/series/([^/]*)/instances/([^/]*)", Protect<RetrieveDicomInstance>);
      Register(root, "studies/([^/]*)/series/([^/]*)/instances/([^/]*)/bulk/(.*)", Protect<RetrieveBulkData>);
      Register(root, "studies/([^/]*)/series/([^/]*)/instances/([^/]*)/metadata", Protect<RetrieveInstanceMetadata>);
      Register(root, "studies/([^/]*)/series/([^/]*)/metadata", Protect<RetrieveSeriesMetadata>);
      Register(root, "studies/([^/]*)/series/([^/]*)/instances/([^/]*)/frames", Protect<RetrieveFrames>);
      Register(root, "studies/([^/]*)/series/([^/]*)/instances/([^/]*)/frames/([^/]*)", Protect<RetrieveFrames>);

      Register(root, "peers", Protect<ListPeers>);
      Register(root, "peers/([^/]*)", Protect<ListPeerOperations>);
      Register(root, "peers/([^/]*)/stow", Protect<StowClient>);
    }
    else
    {
      OrthancPluginLogWarning(context_, "DICOMweb support is disabled");
    }

    // Configure the WADO callback
    if (OrthancPlugins::Configuration::GetBoolValue(configuration_, "EnableWado", true))
    {
      std::string wado = OrthancPlugins::Configuration::GetWadoRoot(configuration_);

      std::string message = "URI to the WADO-URI API: " + wado;
      OrthancPluginLogWarning(context_, message.c_str());

      OrthancPluginRegisterRestCallback(context_, wado.c_str(), Protect<WadoUriCallback>);
    }
    else
    {
      OrthancPluginLogWarning(context_, "WADO-URI support is disabled");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "dicom-web";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_DICOM_WEB_VERSION;
  }
}
