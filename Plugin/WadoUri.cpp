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


#include "WadoUri.h"
#include "Plugin.h"

#include "Configuration.h"

#include <string>


static bool MapWadoToOrthancIdentifier(std::string& orthanc,
                                       char* (*func) (OrthancPluginContext*, const char*),
                                       const std::string& dicom)
{
  OrthancPluginContext* context = OrthancPlugins::Configuration::GetContext();

  char* tmp = func(context, dicom.c_str());

  if (tmp)
  {
    orthanc = tmp;
    OrthancPluginFreeString(context, tmp);
    return true;
  }
  else
  {
    return false;
  }
}


static bool LocateInstance(std::string& instance,
                           std::string& contentType,
                           const OrthancPluginHttpRequest* request)
{
  OrthancPluginContext* context = OrthancPlugins::Configuration::GetContext();

  std::string requestType, studyUid, seriesUid, objectUid;

  for (uint32_t i = 0; i < request->getCount; i++)
  {
    std::string key(request->getKeys[i]);
    std::string value(request->getValues[i]);

    if (key == "studyUID")
    {
      studyUid = value;
    }
    else if (key == "seriesUID")
    {
      seriesUid = value;
    }
    else if (key == "objectUID")  // In WADO-URI, "objectUID" corresponds to "SOPInstanceUID"
    {
      objectUid = value;
    }
    else if (key == "requestType")
    {
      requestType = value;
    }
    else if (key == "contentType")
    {
      contentType = value;
    }
  }

  if (requestType != "WADO")
  {
    OrthancPlugins::Configuration::LogError("WADO-URI: Invalid requestType: \"" + requestType + "\"");
    return false;
  }

  if (objectUid.empty())
  {
    OrthancPlugins::Configuration::LogError("WADO-URI: No SOPInstanceUID provided");
    return false;
  }

  if (!MapWadoToOrthancIdentifier(instance, OrthancPluginLookupInstance, objectUid))
  {
    OrthancPlugins::Configuration::LogError("WADO-URI: No such SOPInstanceUID in Orthanc: \"" + objectUid + "\"");
    return false;
  }

  /**
   * Below are only sanity checks to ensure that the possibly provided
   * "seriesUID" and "studyUID" match that of the provided instance.
   **/

  if (!seriesUid.empty())
  {
    std::string series;
    if (!MapWadoToOrthancIdentifier(series, OrthancPluginLookupSeries, seriesUid))
    {
      OrthancPlugins::Configuration::LogError("WADO-URI: No such SeriesInstanceUID in Orthanc: \"" + seriesUid + "\"");
      return false;
    }
    else
    {
      Json::Value info;
      if (!OrthancPlugins::RestApiGetJson(info, context, "/instances/" + instance + "/series", false) ||
          info["MainDicomTags"]["SeriesInstanceUID"] != seriesUid)
      {
        OrthancPlugins::Configuration::LogError("WADO-URI: Instance " + objectUid + " does not belong to series " + seriesUid);
        return false;
      }
    }
  }
  
  if (!studyUid.empty())
  {
    std::string study;
    if (!MapWadoToOrthancIdentifier(study, OrthancPluginLookupStudy, studyUid))
    {
      OrthancPlugins::Configuration::LogError("WADO-URI: No such StudyInstanceUID in Orthanc: \"" + studyUid + "\"");
      return false;
    }
    else
    {
      Json::Value info;
      if (!OrthancPlugins::RestApiGetJson(info, context, "/instances/" + instance + "/study", false) ||
          info["MainDicomTags"]["StudyInstanceUID"] != studyUid)
      {
        OrthancPlugins::Configuration::LogError("WADO-URI: Instance " + objectUid + " does not belong to study " + studyUid);
        return false;
      }
    }
  }
  
  return true;
}


static void AnswerDicom(OrthancPluginRestOutput* output,
                        const std::string& instance)
{
  OrthancPluginContext* context = OrthancPlugins::Configuration::GetContext();

  std::string uri = "/instances/" + instance + "/file";

  OrthancPlugins::MemoryBuffer dicom(context);
  if (dicom.RestApiGet(uri, false))
  {
    OrthancPluginAnswerBuffer(context, output, 
                              dicom.GetData(), dicom.GetSize(), "application/dicom");
  }
  else
  {
    OrthancPlugins::Configuration::LogError("WADO-URI: Unable to retrieve DICOM file from " + uri);
    throw OrthancPlugins::PluginException(OrthancPluginErrorCode_Plugin);
  }
}


static bool RetrievePngPreview(OrthancPlugins::MemoryBuffer& png,
                               const std::string& instance)
{
  std::string uri = "/instances/" + instance + "/preview";

  if (png.RestApiGet(uri, true))
  {
    return true;
  }
  else
  {
    OrthancPlugins::Configuration::LogError("WADO-URI: Unable to generate a preview image for " + uri);
    return false;
  }
}


static void AnswerPngPreview(OrthancPluginRestOutput* output,
                             const std::string& instance)
{
  OrthancPluginContext* context = OrthancPlugins::Configuration::GetContext();

  OrthancPlugins::MemoryBuffer png(context);
  if (RetrievePngPreview(png, instance))
  {
    OrthancPluginAnswerBuffer(context, output, 
                              png.GetData(), png.GetSize(), "image/png");
  }
  else
  {
    throw OrthancPlugins::PluginException(OrthancPluginErrorCode_Plugin);
  }
}


static void AnswerJpegPreview(OrthancPluginRestOutput* output,
                              const std::string& instance)
{
  OrthancPluginContext* context = OrthancPlugins::Configuration::GetContext();

  // Retrieve the preview in the PNG format
  OrthancPlugins::MemoryBuffer png(context);
  if (!RetrievePngPreview(png, instance))
  {
    throw OrthancPlugins::PluginException(OrthancPluginErrorCode_Plugin);
  }
  
  OrthancPlugins::OrthancImage image(context);
  image.UncompressPngImage(png.GetData(), png.GetSize());
  image.AnswerJpegImage(output, 90 /* quality */);
}


void WadoUriCallback(OrthancPluginRestOutput* output,
                     const char* url,
                     const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::Configuration::GetContext(), output, "GET");
    return;
  }

  std::string instance;
  std::string contentType = "image/jpg";  // By default, JPEG image will be returned
  if (!LocateInstance(instance, contentType, request))
  {
    throw OrthancPlugins::PluginException(OrthancPluginErrorCode_UnknownResource);
  }

  if (contentType == "application/dicom")
  {
    AnswerDicom(output, instance);
  }
  else if (contentType == "image/png")
  {
    AnswerPngPreview(output, instance);
  }
  else if (contentType == "image/jpeg" ||
           contentType == "image/jpg")
  {
    AnswerJpegPreview(output, instance);
  }
  else
  {
    OrthancPlugins::Configuration::LogError("WADO-URI: Unsupported content type: \"" + contentType + "\"");
    throw OrthancPlugins::PluginException(OrthancPluginErrorCode_BadRequest);
  }
}
