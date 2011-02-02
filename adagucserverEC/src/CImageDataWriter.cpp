#include "CImageDataWriter.h"


const char * CImageDataWriter::className = "CImageDataWriter";
const char * CImageDataWriter::RenderMethodStringList="nearest,bilinear,contour,shadedcontour,shaded,vector,vectorcontour,nearestcontour,bilinearcontour,vectorcontourshaded";
CImageDataWriter::CImageDataWriter(){
  mode = 0;
}

CImageDataWriter::RenderMethodEnum CImageDataWriter::getRenderMethodFromString(CT::string *renderMethodString){
  RenderMethodEnum renderMethod;
  if(renderMethodString->equals("bilinear"))renderMethod=bilinear;
  else if(renderMethodString->equals("nearest"))renderMethod=nearest;
  else if(renderMethodString->equals("vector"))renderMethod=vector;
  else if(renderMethodString->equals("shaded"))renderMethod=shaded;
  else if(renderMethodString->equals("contour"))renderMethod=contour;
  else if(renderMethodString->equals("shadedcontour"))renderMethod=contourshaded;
  else if(renderMethodString->equals("vectorcontour"))renderMethod=vectorcontour;
  else if(renderMethodString->equals("nearestcontour"))renderMethod=nearestcontour;
  else if(renderMethodString->equals("bilinearcontour"))renderMethod=bilinearcontour;
  else if(renderMethodString->equals("vectorcontourshaded"))renderMethod=vectorcontourshaded;
  
  return renderMethod;
}

int CImageDataWriter::_setTransparencyAndBGColor(CServerParams *srvParam,CDrawImage* drawImage){
  //drawImage->setTrueColor(true);
  //Set transparency
  if(srvParam->Transparent==true){
    drawImage->enableTransparency(true);
  }else{
    drawImage->enableTransparency(false);
    //Set BGColor
    if(srvParam->BGColor.length()>0){
      if(srvParam->BGColor.length()!=8){
        CDBError("BGCOLOR does not comply to format 0xRRGGBB");
        return 1;
      }
      if(srvParam->BGColor.c_str()[0]!='0'||srvParam->BGColor.c_str()[1]!='x'){
        CDBError("BGCOLOR does not comply to format 0xRRGGBB");
        return 1;
      }
      unsigned char hexa[8];
      memcpy(hexa,srvParam->BGColor.c_str()+2,6);
      for(size_t j=0;j<6;j++){
        hexa[j]-=48;
        if(hexa[j]>16)hexa[j]-=7;
      }
      drawImage->setBGColor(hexa[0]*16+hexa[1],hexa[2]*16+hexa[3],hexa[4]*16+hexa[5]);
    }else{
      drawImage->setBGColor(255,255,255);
    }
  }
  return 0;
}

int CImageDataWriter::drawCascadedWMS(const char *service,const char *layers,bool transparent){
  return 0;
#ifdef ENABLE_CURL
  
  bool trueColor=drawImage.getTrueColor();
  transparent=true;
  CT::string url=service;
  url.concat("SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&");
  if(trueColor==false)url.concat("FORMAT=image/gif");
  else url.concat("FORMAT=image/png");
  //&BBOX=50.943396226415075,-4.545656817372752,118.8679245283019,57.6116945532218
  if(transparent)url.printconcat("&TRANSPARENT=TRUE");
  url.printconcat("&WIDTH=%d",drawImage.Geo->dWidth);
  url.printconcat("&HEIGHT=%d",drawImage.Geo->dHeight);
  url.printconcat("&BBOX=%0.4f,%0.4f,%0.4f,%0.4f",drawImage.Geo->dfBBOX[0],
                  drawImage.Geo->dfBBOX[1],
                  drawImage.Geo->dfBBOX[2],
                  drawImage.Geo->dfBBOX[3]);
  url.printconcat("&SRS=%s",drawImage.Geo->CRS.c_str());
  url.printconcat("&LAYERS=%s",layers);
  CDBDebug(url.c_str());
  gdImagePtr gdImage;
  MyCURL myCURL;
  myCURL.getGDImageField(url.c_str(),gdImage);
  if(gdImage&&1==1){
    //int w=gdImageSX(gdImage);
    //int h=gdImageSY(gdImage);
    int transpColor=gdImageGetTransparent(gdImage);
    for(int y=0;y<drawImage.Geo->dHeight;y++){
      for(int x=0;x<drawImage.Geo->dWidth;x++){
        int color = gdImageGetPixel(gdImage, x, y);
        if(color!=transpColor&&127!=gdImageAlpha(gdImage,color)){
          if(transparent)
            drawImage.setPixelTrueColor(x,y,gdImageRed(gdImage,color),gdImageGreen(gdImage,color),gdImageBlue(gdImage,color),255-gdImageAlpha(gdImage,color)*2);
          else
            drawImage.setPixelTrueColor(x,y,gdImageRed(gdImage,color),gdImageGreen(gdImage,color),gdImageBlue(gdImage,color));
        }
      }
    }
    gdImageDestroy(gdImage);
  }
  return 0;
#elseif
  CDBError("CURL not enabled");
  return 1;
#endif
  return 0;
}

int CImageDataWriter::init(CServerParams *srvParam,CDataSource *dataSource, int NrOfBands){
  if(mode!=0){CDBError("Already initialized");return 1;}
  this->srvParam=srvParam;
  if(_setTransparencyAndBGColor(this->srvParam,&drawImage)!=0)return 1;
  if(srvParam->Format.indexOf("24")>0||srvParam->Styles.indexOf("HQ")>0){
    drawImage.setTrueColor(true);
    drawImage.setAntiAliased(true);
  }
  
  //Set font location
  if(srvParam->cfg->Font.size()!=0){
    if(srvParam->cfg->Font[0]->attr.location.c_str()!=NULL){
      drawImage.setTTFFontLocation(srvParam->cfg->Font[0]->attr.location.c_str());
      //CDBError("Font %s",srvParam->cfg->Font[0]->attr.location.c_str());
      //return 1;

    }else {
      CDBError("Font configured, but no attribute location was given");
      return 1;
    }
  }
  
  

  
  //drawImage.setTrueColor(true);
  //drawImage.setAntiAliased(true);
  /*drawImage.setTrueColor(true);
  drawImage.setAntiAliased(true);*/
  mode=1;
  animation = 0;
  nrImagesAdded = 0;
  requestType=srvParam->requestType;
  if(requestType==REQUEST_WMS_GETMAP){
    status = drawImage.createImage(srvParam->Geo);
    if(status != 0) return 1;
  }
  if(requestType==REQUEST_WMS_GETLEGENDGRAPHIC){
    //drawImage.setAntiAliased(false);
    //drawImage.setTrueColor(false);
    status = drawImage.createImage(LEGEND_WIDTH,LEGEND_HEIGHT);
    if(status != 0) return 1;
  }
  if(requestType==REQUEST_WMS_GETFEATUREINFO){
    drawImage.Geo->copy(srvParam->Geo);
    getFeatureInfoResult.copy("");
    getFeatureInfoHeader.copy("");
    return 0;
  }
  
  int dLegendIndex = initializeLegend(srvParam,dataSource);

  if(dLegendIndex==-1){
    CDBError("Unable to initialize legend for dataSource %s",dataSource->cfgLayer->Name[0]->value.c_str());
    return 1;
  }

  
  status = drawImage.createGDPalette(srvParam->cfg->Legend[dLegendIndex]);
  if(status != 0){
    CDBError("Unknown palette type for %s",srvParam->cfg->Legend[dLegendIndex]->attr.name.c_str());
    return 1;
  }
  if(requestType==REQUEST_WMS_GETMAP){
    /*---------Add cascaded background map now------------------------------------*/
    //drawCascadedWMS("http://geoservices.knmi.nl/cgi-bin/worldmaps.cgi?","world_raster",false);
    //drawCascadedWMS("http://bhlbontw.knmi.nl/rcc/download/ensembles/cgi-bin/basemaps.cgi?","world_eca,country",true);
    /*----------------------------------------------------------------------------*/
  }
  return 0;
}
int CImageDataWriter::initializeLegend(CServerParams *srvParam,CDataSource *dataSource){
  if(srvParam==NULL){
    CDBError("srvParam==NULL");
    return 1;
  }
  int dLegendIndex=-1;
  if(_setTransparencyAndBGColor(srvParam,&drawImage)!=0)return 1;
  

  /* GET LEGEND INFORMATION From the layer itself*/
  //Get legend for the layer by using layers legend
  if(dataSource->cfgLayer->Legend.size()!=0){
    if(srvParam->requestType==REQUEST_WMS_GETMAP||srvParam->requestType==REQUEST_WMS_GETLEGENDGRAPHIC){
      for(size_t j=0;j<srvParam->cfg->Legend.size()&&dLegendIndex==-1;j++){
        if(dataSource->cfgLayer->Legend[0]->value.equals(
           srvParam->cfg->Legend[j]->attr.name.c_str())){
          dLegendIndex=j;
          break;
           }
      }
    }
  }
  //Get Legend settings
  if(dataSource->cfgLayer->Scale.size()>0){
    dataSource->legendScale=parseFloat(dataSource->cfgLayer->Scale[0]->value.c_str());
  }
  if(dataSource->cfgLayer->Offset.size()>0){
    dataSource->legendOffset=parseFloat(dataSource->cfgLayer->Offset[0]->value.c_str());
  }
  if(dataSource->cfgLayer->Log.size()>0){
    dataSource->legendLog=parseFloat(dataSource->cfgLayer->Log[0]->value.c_str());
  }
  if(dataSource->cfgLayer->ValueRange.size()>0){
    dataSource->legendValueRange=1;
    dataSource->legendLowerRange=parseFloat(dataSource->cfgLayer->ValueRange[0]->attr.min.c_str());
    dataSource->legendUpperRange=parseFloat(dataSource->cfgLayer->ValueRange[0]->attr.max.c_str());
  }

  /* GET STYLE INFORMATION (OPTIONAL) */
  //Get style information (if provided)
  int dLayerStyleIndex=-1;//Equals styles=default (Nearest neighbour rendering)
  CT::string layerStyleName;
  
  //Default rendermethod is nearest
  renderMethod=nearest;
  
  //Try to find the default rendermethod from the layers style object
  if(dataSource->cfgLayer->Styles.size()==1){
    CT::string styles(dataSource->cfgLayer->Styles[0]->value.c_str());
    CT::string *layerstyles = styles.split(",");
    if(layerstyles->count>0){
      dLayerStyleIndex=0;
      layerStyleName.copy(&layerstyles[0]);
      if(srvParam->cfg->Style.size()>0){
        size_t j=0;
        for(j=0;j<srvParam->cfg->Style.size();j++){
          if(layerStyleName.equals(srvParam->cfg->Style[j]->attr.name.c_str())){
            break;
          }
        }
//        CDBDebug("j=%d",j);
        if(srvParam->cfg->Style[j]->RenderMethod.size()==1){
          CT::string renderMethodList(srvParam->cfg->Style[j]->RenderMethod[0]->value.c_str());
          CT::string *renderMethods = renderMethodList.split(",");
          if(renderMethods->count>0){
            renderMethod=getRenderMethodFromString(&renderMethods[0]);
          }
          delete[] renderMethods;
        }
      }
    }
    delete[] layerstyles ;
  }else return dLegendIndex;
  
  //If a rendermethod is given in the layers config, use the first one as default
  if(dataSource->cfgLayer->RenderMethod.size()==1){
    CT::string tmp(dataSource->cfgLayer->RenderMethod[0]->value.c_str());
    CT::string *renderMethodList = tmp.split(",");
    renderMethod=getRenderMethodFromString(&renderMethodList[0]);
    delete[] renderMethodList;
  }
  //styles=temperature/nearestneighbour
  //Get legend for the layer by using layers style
  CT::string *requestStyle=srvParam->Styles.split(",");
  bool isDefaultStyle=false;
  if(requestStyle->count>0){
    if(requestStyle[0].length()>0){
      if(requestStyle[0].equals("default")){
        isDefaultStyle=true;
      }
      //if(!requestStyle[0].equals("default"))
      {
        //requestStyle[0].copy("temperature/shadedcontour/HQ");
        if(dataSource->cfgLayer->Styles.size()==1){
          CT::string *legendStyle=NULL;
          if(srvParam->requestType==REQUEST_WMS_GETMAP||srvParam->requestType==REQUEST_WMS_GETLEGENDGRAPHIC){
            CT::string styles(dataSource->cfgLayer->Styles[0]->value.c_str());
            CT::string *layerstyles = styles.split(",");
            //If default, take the first style...
            if(isDefaultStyle){
              requestStyle[0].copy(&layerstyles[0]);
            }
            legendStyle=requestStyle[0].split("/");
            //if(!requestStyle[0].equals("default")){
              for(int j=0;j<layerstyles->count;j++){
                if(layerstyles[j].equals(legendStyle[0].c_str())){
                  layerStyleName.copy(&layerstyles[j]);
                  dLayerStyleIndex=j;
                  break;
                }
              }
            /*}else {
              dLayerStyleIndex=0;
              layerStyleName.copy(&layerstyles[dLayerStyleIndex]);
              delete[] legendStyle;
              legendStyle=layerStyleName.split("/");
              CDBDebug("layerStyleName %s",layerStyleName.c_str());
          }*/
            delete[] layerstyles;
          }
          if(legendStyle!=NULL){
            //Find the render method:
            if(legendStyle->count==2||legendStyle->count==3){
              if(legendStyle[1].length()>0){
                //CDBDebug("Rendermethod = %s",legendStyle[1].c_str());
                renderMethod=getRenderMethodFromString(&legendStyle[1]);
              }
            }
          }
          
          delete[] legendStyle;
        }
        
        
        
        //The style was not found in the layer
        if(dLayerStyleIndex==-1&&dataSource->dLayerType!=CConfigReaderLayerTypeStyled){
          if(isDefaultStyle==true){
            dLayerStyleIndex=0;
            return 0;
          }
          CDBError("Style '%s' was not found in the layers configuration",requestStyle[0].c_str());
          return -1;
        }
      }
    }
  }
  /*if(dLayerStyleIndex==-1){
        //If no style information like legend, scale and offset is not given in the config
        //Than the use the first index of the style element instead as style=default
  if(dLegendIndex==-1){
  if(dataSource->cfgLayer->Styles.size()==1){
  CT::string styles(dataSource->cfgLayer->Styles[0]->value.c_str());
  CT::string *layerstyles = styles.split(",");
  if(layerstyles->count>0){
          //renderMethod=nearest;
  dLayerStyleIndex=0;
  layerStyleName.copy(&layerstyles[0]);
}
  delete[] layerstyles;
}else{
  CDBError("No style information could be found in the configuration or in the layers configuration for layer %s",dataSource->getLayerName());
  return 1;
}
}
}*/
  delete[] requestStyle;
  
  int dConfigStyleIndex=-1;//Equals styles=default (Nearest neighbour rendering)
  //Get the servers style index from the name
  if(dLayerStyleIndex>-1){
    //CDBDebug("dLayerStyleIndex %d - %d",dLayerStyleIndex,srvParam->cfg->Style.size());
    //Get the servers style index from the name
//    CDBDebug("Found style %s in the layers config, now searching the servers config.",layerStyleName.c_str());

    if(srvParam->cfg->Style.size()>0){
      if(srvParam->requestType==REQUEST_WMS_GETMAP||srvParam->requestType==REQUEST_WMS_GETLEGENDGRAPHIC){
        for(size_t j=0;j<srvParam->cfg->Style.size()&&dConfigStyleIndex==-1;j++){
          if(layerStyleName.equals(srvParam->cfg->Style[j]->attr.name.c_str())){
            dConfigStyleIndex=j;
            break;
          }
        }
      }
    }else{
      CDBError("Styles is configured in the Layer, but no Style is configured in the servers configuration file");
      return -1;
    }
    if(dConfigStyleIndex!=-1){
      if(isDefaultStyle){
        CServerConfig::XMLE_Style * cfgStyle = srvParam->cfg->Style[dConfigStyleIndex];
        if(cfgStyle->RenderMethod.size()==1){
          //renderMethod=getRenderMethodFromString(&renderMethods[0]);
          CT::string rm=cfgStyle->RenderMethod[0]->value.c_str();
          CT::string *rms=rm.split(",");
          if(rms->count>0){
            CT::string *rms2=rms[0].split("/");
            renderMethod=getRenderMethodFromString(&rms2[0]);
            delete[] rms2;
          }
          delete[] rms;
          //CDBDebug("Found style '%s' with index %d in configuration",layerStyleName.c_str(),dConfigStyleIndex);
        }
      }
    }else{
      CDBError("Unable to find style '%s' in configuration",layerStyleName.c_str());
      return -1;
    }
  }
   //If no legend is yet found, and no style is provided, we assume styles=default
  if(dConfigStyleIndex==-1&&dLegendIndex==-1&&srvParam->cfg->Style.size()>0){
    dConfigStyleIndex=0;
  }
  
  if(dConfigStyleIndex!=-1&&srvParam->cfg->Style.size()>0){
    CServerConfig::XMLE_Style * cfgStyle = srvParam->cfg->Style[dConfigStyleIndex];
    /* GET LEGEND INFORMATION FROM STYLE OBJECT */
    //Get legend for the layer by using layers legend
    if(dLegendIndex==-1){
      dLegendIndex = -1;
      if(srvParam->requestType==REQUEST_WMS_GETMAP||srvParam->requestType==REQUEST_WMS_GETLEGENDGRAPHIC){
        for(size_t j=0;j<srvParam->cfg->Legend.size()&&dLegendIndex==-1;j++){
          if(cfgStyle->Legend.size()==0){
            CDBError("No Legend found for style %s",cfgStyle->attr.name.c_str());
            return -1;
          }
          if(cfgStyle->Legend[0]->value.equals(srvParam->cfg->Legend[j]->attr.name.c_str())){
            dLegendIndex=j;
            break;
          }
        }
      }
    }
   //Get Legend settings
    
    if(cfgStyle->Scale.size()>0){
      dataSource->legendScale=parseFloat(cfgStyle->Scale[0]->value.c_str());
    }
    if(cfgStyle->Offset.size()>0){
      dataSource->legendOffset=parseFloat(cfgStyle->Offset[0]->value.c_str());
    }
    if(cfgStyle->Log.size()>0){
      dataSource->legendLog=parseFloat(cfgStyle->Log[0]->value.c_str());
    }
    if(cfgStyle->ValueRange.size()>0){
      dataSource->legendValueRange=1;
      dataSource->legendLowerRange=parseFloat(cfgStyle->ValueRange[0]->attr.min.c_str());
      dataSource->legendUpperRange=parseFloat(cfgStyle->ValueRange[0]->attr.max.c_str());
    }
    
    //Legend settings can always be overriden in the layer itself!
    if(dataSource->cfgLayer->Scale.size()>0){
      dataSource->legendScale=parseFloat(dataSource->cfgLayer->Scale[0]->value.c_str());
    }
    if(dataSource->cfgLayer->Offset.size()>0){
      dataSource->legendOffset=parseFloat(dataSource->cfgLayer->Offset[0]->value.c_str());
    }
    if(dataSource->cfgLayer->Log.size()>0){
      dataSource->legendLog=parseFloat(dataSource->cfgLayer->Log[0]->value.c_str());
    }
    if(dataSource->cfgLayer->ValueRange.size()>0){
      dataSource->legendValueRange=1;
      dataSource->legendLowerRange=parseFloat(dataSource->cfgLayer->ValueRange[0]->attr.min.c_str());
      dataSource->legendUpperRange=parseFloat(dataSource->cfgLayer->ValueRange[0]->attr.max.c_str());
    }

    
    //Get contour info
    shadeInterval=0.0f;contourIntervalL=0.0f;contourIntervalH=0.0f;smoothingFilter=1;
    if(cfgStyle->ContourIntervalL.size()>0)
      contourIntervalL=parseFloat(cfgStyle->ContourIntervalL[0]->value.c_str());
    if(cfgStyle->ContourIntervalH.size()>0)
      contourIntervalH=parseFloat(cfgStyle->ContourIntervalH[0]->value.c_str());
    shadeInterval=contourIntervalL;
    if(cfgStyle->ShadeInterval.size()>0)
      shadeInterval=parseFloat(cfgStyle->ShadeInterval[0]->value.c_str());
    if(cfgStyle->SmoothingFilter.size()>0)
      smoothingFilter=parseInt(cfgStyle->SmoothingFilter[0]->value.c_str());
    //Get contourtextinfo
    textScaleFactor=1.0f;textOffsetFactor=0.0f;
    
    if(cfgStyle->ContourText.size()>0){
      if(cfgStyle->ContourText[0]->attr.scale.c_str()!=NULL){
        textScaleFactor=parseFloat(cfgStyle->ContourText[0]->attr.scale.c_str());
      }
      if(cfgStyle->ContourText[0]->attr.offset.c_str()!=NULL)
        textOffsetFactor=parseFloat(cfgStyle->ContourText[0]->attr.offset.c_str());
    }
  }
  if(contourIntervalL<=0.0f||contourIntervalH<=0.0f){
    if(renderMethod==contour||
       renderMethod==bilinearcontour||
       renderMethod==nearestcontour){
      renderMethod=nearest;
       }
  }
  
  if(dLegendIndex==-1){
    CDBError("No legend found for layer %s", dataSource->getLayerName());
    return -1;
  }

  return dLegendIndex;
}
int CImageDataWriter::createLegend(CDataSource *dataSource){
  status = createLegend(dataSource,&drawImage);
  //if(status != 0)return 1;
  return status;//end();
}

double CImageDataWriter::convertValue(CDFType type,void *data,size_t ptr){
  double pixel = 0.0f;
  if(type==CDF_CHAR)pixel=((char*)data)[ptr];
  if(type==CDF_UBYTE)pixel=((unsigned char*)data)[ptr];
  if(type==CDF_SHORT)pixel=((short*)data)[ptr];
  if(type==CDF_USHORT)pixel=((unsigned short*)data)[ptr];
  if(type==CDF_INT)pixel=((int*)data)[ptr];
  if(type==CDF_UINT)pixel=((unsigned int*)data)[ptr];
  if(type==CDF_FLOAT)pixel=((float*)data)[ptr];
  if(type==CDF_DOUBLE)pixel=((double*)data)[ptr];
  return pixel;
}
void CImageDataWriter::setValue(CDFType type,void *data,size_t ptr,double pixel){
  if(type==CDF_CHAR)((char*)data)[ptr]=(char)pixel;
  if(type==CDF_UBYTE)((unsigned char*)data)[ptr]=(unsigned char)pixel;
  if(type==CDF_SHORT)((short*)data)[ptr]=(short)pixel;
  if(type==CDF_USHORT)((unsigned short*)data)[ptr]=(unsigned short)pixel;
  if(type==CDF_INT)((int*)data)[ptr]=(int)pixel;
  if(type==CDF_UINT)((unsigned int*)data)[ptr]=(unsigned int)pixel;
  if(type==CDF_FLOAT)((float*)data)[ptr]=(float)pixel;
  if(type==CDF_DOUBLE)((double*)data)[ptr]=(double)pixel;
}
int CImageDataWriter::getFeatureInfo(CDataSource *dataSource,int dX,int dY){
  CT::string tempResult;
//  status  = imageWarper.getFeatureInfo(&getFeatureInfoHeader,&temp,dataSource,drawImage.Geo,dX,dY);
  
  //int CImageWarper::getFeatureInfo(CT::string *Header,CT::string *Result,CDataSource *dataSource,CGeoParams *GeoDest,int dX, int dY){

  char szTemp[MAX_STR_LEN+1];
  if(dataSource==NULL){
    CDBError("dataSource == NULL");
    return 1;
  }
  int status;
  CDataReader reader;
  
  CT::string cacheLocation;srvParam->getCacheDirectory(&cacheLocation);
  
  status = reader.open(dataSource,CNETCDFREADER_MODE_OPEN_ALL,cacheLocation.c_str());
  if(status!=0){
    CDBError("Could not open file: %s",dataSource->getFileName());
    return 1;
  }
  status = imageWarper.initreproj(dataSource,drawImage.Geo,&srvParam->cfg->Projection);
  if(status!=0){
    CDBError("initreproj failed");
    reader.close();
    return 1;
  }
  tempResult.copy("");
  getFeatureInfoHeader.copy("");
  double x,y,sx,sy,CoordX,CoordY;
  int imx,imy;
  sx=dX;
  sy=dY;
  //printf("Native XY (%f : %f)\n<br>",sx,sy);
  x=double(sx)/double(drawImage.Geo->dWidth);
  y=double(sy)/double(drawImage.Geo->dHeight);
  x*=(drawImage.Geo->dfBBOX[2]-drawImage.Geo->dfBBOX[0]);
  y*=(drawImage.Geo->dfBBOX[1]-drawImage.Geo->dfBBOX[3]);
  x+=drawImage.Geo->dfBBOX[0];
  y+=drawImage.Geo->dfBBOX[3];
  //printf("%s%c%c\n","Content-Type:text/html",13,10);
  CoordX=x;
  CoordY=y;
  imageWarper.reprojpoint(x,y);
  //printf("Projected XY (%f : %f)\n<br>",x,y);
  x-=dataSource->dfBBOX[0];
  y-=dataSource->dfBBOX[1];
  x/=(dataSource->dfBBOX[2]-dataSource->dfBBOX[0]);
  y/=(dataSource->dfBBOX[3]-dataSource->dfBBOX[1]);
  x*=double(dataSource->dWidth);
  y*=double(dataSource->dHeight);
  imx=(int)x;
  imy=dataSource->dHeight-(int)y-1;
// printf("Image XY (%d : %d)\n<br>",imx,imy);
  imageWarper.closereproj();
  getFeatureInfoHeader.concat("<b>");
  for(size_t j=0;j<dataSource->dataObject.size();j++){
    if(dataSource->dataObject.size()>1)getFeatureInfoHeader.printconcat("(%d) ",j);
    getFeatureInfoHeader.concat(dataSource->dataObject[j]->variableName.c_str());
    if(j+1<dataSource->dataObject.size())getFeatureInfoHeader.concat(",");
  }
  getFeatureInfoHeader.concat("</b><br>");
  snprintf(szTemp,MAX_STR_LEN,"coordinates (%0.2f , %0.2f)<br>\n",CoordX,CoordY);
  getFeatureInfoHeader.concat(szTemp);

  if(imx>=0&&imy>=0&&imx<dataSource->dWidth&&imy<dataSource->dHeight){
    for(size_t j=0;j<dataSource->dataObject.size();j++){
      size_t ptr=imx+imy*dataSource->dWidth;
      double pixel=0;
      pixel = convertValue(dataSource->dataObject[j]->dataType,dataSource->dataObject[j]->data,ptr);
      status = reader.getTimeString(szTemp);
      if(status != 0){
        tempResult.printconcat("status = %d: ",status);
      }
      else{
        tempResult.concat(szTemp);if(strlen(szTemp)>1)tempResult.concat(": ");
      }
      if((pixel!=dataSource->dataObject[j]->dfNodataValue&&dataSource->dataObject[j]->hasNodataValue==true)||dataSource->dataObject[0]->hasNodataValue==false){
        floatToString(szTemp,MAX_STR_LEN,pixel);
        tempResult.concat("<b>");
        if(dataSource->dataObject.size()>1)tempResult.printconcat("(%d) ",j);
        tempResult.concat(szTemp);
        tempResult.concat("</b>");
  
        if(dataSource->dataObject[j]->units.length()!=0){
          snprintf(szTemp,MAX_STR_LEN," %s",dataSource->dataObject[j]->units.c_str());
          tempResult.concat(szTemp);
        }
        tempResult.concat("<br>\n");
  
      }
      else {
        tempResult.concat("no data<br>\n");
      }
    }
  }
  reader.close();
  getFeatureInfoResult.concat(&tempResult);
  return 0;
}
int CImageDataWriter::createAnimation(){
  printf("%s%c%c\n","Content-Type:image/gif",13,10);
  drawImage.beginAnimation();
  animation = 1;
  return 0;
}
void CImageDataWriter::setDate(const char *szTemp){
  drawImage.setTextStroke(szTemp, strlen(szTemp),drawImage.Geo->dWidth-170,5,240,254,0);
}

int CImageDataWriter::warpImage(CDataSource *dataSource,CDrawImage *drawImage){
  
  int status;
  //Open the data of this dataSource
  //CDBDebug("opening:");
  CDataReader reader;
  //CDBDebug("!");
  CT::string cacheLocation;srvParam->getCacheDirectory(&cacheLocation);
  //CDBDebug("!");
  status = reader.open(dataSource,CNETCDFREADER_MODE_OPEN_ALL,cacheLocation.c_str());
  //CDBDebug("!");
  if(status!=0){
    CDBError("Could not open file: %s",dataSource->getFileName());
    return 1;
  }
//  CDBDebug("opened");
  //Initialize projection algorithm
  status = imageWarper.initreproj(dataSource,drawImage->Geo,&srvParam->cfg->Projection);
  if(status!=0){
    CDBError("initreproj failed");
    reader.close();
    return 1;
  }
  
#ifdef MEASURETIME
  StopWatch_Stop("warp start");
#endif

/*if(renderMethod==nearest){CDBDebug("nearest");}
if(renderMethod==bilinear){CDBDebug("bilinear");}
if(renderMethod==bilinearcontour){CDBDebug("bilinearcontour");}
if(renderMethod==nearestcontour){CDBDebug("nearestcontour");}
if(renderMethod==contour){CDBDebug("contour");}*/

  CImageWarperRenderInterface *imageWarperRenderer;
  if(renderMethod==nearest||renderMethod==nearestcontour){
    imageWarperRenderer = new CImgWarpNearestNeighbour();
    imageWarperRenderer->render(&imageWarper,dataSource,drawImage);
    delete imageWarperRenderer;
  }
  
  if(renderMethod==nearestcontour||
     renderMethod==bilinear||
     renderMethod==bilinearcontour||
     renderMethod==contour||
     renderMethod==vector||
     renderMethod==vectorcontour||
     renderMethod==shaded||
     renderMethod==contourshaded||
     renderMethod==vectorcontourshaded
    )
  {
    imageWarperRenderer = new CImgWarpBilinear();
    CT::string bilinearSettings;
    bool drawMap=false;
    bool drawContour=false;
    bool drawVector=false;
    bool drawShaded=false;
    if(renderMethod==bilinear||renderMethod==bilinearcontour)drawMap=true;
    if(renderMethod==bilinearcontour)drawContour=true;
    if(renderMethod==nearestcontour)drawContour=true;
    if(renderMethod==contour||renderMethod==contourshaded||renderMethod==vectorcontour||renderMethod==vectorcontourshaded)drawContour=true;
    if(renderMethod==vector||renderMethod==vectorcontour||renderMethod==vectorcontourshaded)drawVector=true;
    if(renderMethod==shaded||renderMethod==contourshaded||renderMethod==vectorcontourshaded)drawShaded=true;
    if(drawMap==true)bilinearSettings.printconcat("drawMap=true;");
    if(drawVector==true)bilinearSettings.printconcat("drawVector=true;");
    if(drawShaded==true)bilinearSettings.printconcat("drawShaded=true;");
    if(drawContour==true)bilinearSettings.printconcat("drawContour=true;");
    if(drawContour==true||drawShaded==true){
      bilinearSettings.printconcat("shadeInterval=%f;contourSmallInterval=%f;contourBigInterval=%f;",
                                   shadeInterval,contourIntervalL,contourIntervalH);
      bilinearSettings.printconcat("textScaleFactor=%f;textOffsetFactor=%f;",
                                   textScaleFactor,textOffsetFactor);
      bilinearSettings.printconcat("smoothingFilter=%d;",smoothingFilter);
    }
//    CDBDebug("bilinearSettings.c_str() %s",bilinearSettings.c_str());
    imageWarperRenderer->set(bilinearSettings.c_str());
    imageWarperRenderer->render(&imageWarper,dataSource,drawImage);
    delete imageWarperRenderer;
  }
  
#ifdef MEASURETIME
  StopWatch_Stop("warp  finished");
#endif
  imageWarper.closereproj();
  reader.close();
  return 0;
}

// Virtual functions
int CImageDataWriter::calculateData(std::vector <CDataSource*>&dataSources){
  /**
  This style has a special *custom* non WMS syntax:
  First style: represents how the boolean results must be combined
  Keywords: "and","or"
  Example: "and" for two layers, "and_and" for three layers
  Second and N+2 style: represents how the boolean map is created and which time is required
  Keywords: "and","between","notbetween","lessthan","greaterthan","time","|"
  Example: between_10.0_and_20.0|time_1990-01-01T00:00:00Z
  Note: after "|" always a time is specified with time_
   */

//  int status;
  CDBDebug("calculateData");

  if(animation==1&&nrImagesAdded>1){
    drawImage.addImage(30);
  }
  nrImagesAdded++;
  // draw the Image
  //for(size_t j=1;j<dataSources.size();j++)
  {
    CDataSource *dataSource;
    
    
    /**************************************************/
    int status;
    bool hasFailed=false;
    //Open the corresponding data of this dataSource, with the datareader
    std::vector <CDataReader*> dataReaders;
    for(size_t i=0;i<dataSources.size();i++){
      dataSource=dataSources[i];
      CDataReader *reader = new CDataReader ();
      dataReaders.push_back(reader);
      CT::string cacheLocation;srvParam->getCacheDirectory(&cacheLocation);
      status = reader->open(dataSource,CNETCDFREADER_MODE_OPEN_ALL,cacheLocation.c_str());
      CDBDebug("Opening %s",dataSource->getFileName());
      if(status!=0){CDBError("Could not open file: %s",dataSource->getFileName());  hasFailed=true; }
    }
    //Initialize projection algorithm
    dataSource=dataSources[0];
    
    if(hasFailed==false){
      status = imageWarper.initreproj(dataSource,drawImage.Geo,&srvParam->cfg->Projection);
      if(status!=0){
        CDBError("initreproj failed");
        hasFailed=true;
      }
    }
    if(hasFailed==false){
      //Start modifying the data using the specific style
      
      enum ConditionalOperator{ myand,myor,between,notbetween,lessthan,greaterthan};
      
      ConditionalOperator combineBooleanMapExpression[dataSources.size()-1];
      ConditionalOperator inputMapExpression[dataSources.size()];
      float inputMapExprValuesLow[dataSources.size()];
      float inputMapExprValuesHigh[dataSources.size()];
      
      CT::string *layerStyles = srvParam->Styles.split(",");
      CT::string style;
//      bool errorOccured=false;
      for(size_t j=0;j<dataSources.size();j++){
        int numberOfValues = 1;
        CT::string *_style = layerStyles[j].split("|");
        style.copy(&_style[0]);
        CDBDebug("STYLE == %s",style.c_str());
        if(j==0){
          //Find the conditional expression for the first layer (the boolean map)
          CT::string *conditionals = style.split("_");
          if(!conditionals[0].equals("default")&&conditionals->count!=(int)dataSources.size()-2){
            CDBError("Incorrect number of conditional operators specified: %d  (expected %d)",
                     conditionals->count,dataSources.size()-2);
            hasFailed=true;
          }else{
            for(int i=0;i<conditionals->count;i++){
              combineBooleanMapExpression[i]=myand;
              if(conditionals[i].equals("and"))combineBooleanMapExpression[i]=myand;
              if(conditionals[i].equals("or"))combineBooleanMapExpression[i]=myor;
            }
            
          }
          delete[] conditionals;
        }else{
          inputMapExpression[j]=between;
          CT::string exprVal("0.0");
          //Find the expressin types:
          if(style.indexOf("between_")==0){
            inputMapExpression[j]=between;
            exprVal.copy(style.c_str()+8);
            numberOfValues=2;
          }
          if(style.indexOf("notbetween_")==0){
            inputMapExpression[j]=notbetween;
            exprVal.copy(style.c_str()+11);
            numberOfValues=2;
          }
          if(style.indexOf("lessthan_")==0){
            inputMapExpression[j]=lessthan;
            exprVal.copy(style.c_str()+9);
            numberOfValues=1;
          }
          if(style.indexOf("greaterthan_")==0){
            inputMapExpression[j]=greaterthan;
            exprVal.copy(style.c_str()+12);
            numberOfValues=1;
          }
          CT::string *LH=exprVal.split("_and_");
          if(LH->count!=numberOfValues){
            CDBError("Invalid number of values in expression '%s'",style.c_str());
            hasFailed=true;
          }else{
            inputMapExprValuesLow[j]=LH[0].toFloat();
            if(numberOfValues==2){
              inputMapExprValuesHigh[j]=LH[1].toFloat();
            }
          }
          delete[] LH;
          if(numberOfValues==1){
            CDBDebug("'%f'",inputMapExprValuesLow[j]);
          }
          if(numberOfValues==2){
            CDBDebug("'%f' and '%f'",inputMapExprValuesLow[j],inputMapExprValuesHigh[j]);
          }
        }
        delete [] _style;
      }
      
      CDBDebug("Start creating the boolean map");
      double pixel[dataSources.size()];
      bool conditialMap[dataSources.size()];
      for(int y=0;y<dataSource->dHeight;y++){
        for(int x=0;x<dataSource->dWidth;x++){
          size_t ptr=x+y*dataSource->dWidth;
          for(size_t j=1;j<dataSources.size();j++){
            CDataSource *dsj = dataSources[j];
            int xj=int((float(x)/float(dataSource->dWidth))*float(dsj->dWidth));
            int yj=int((float(y)/float(dataSource->dHeight))*float(dsj->dHeight));
            if(dsj->dfBBOX[1]>dsj->dfBBOX[3])yj=dsj->dHeight-yj-1;
            size_t ptrj=xj+yj*dsj->dWidth;
            
            pixel[j] = convertValue(dsj->dataObject[0]->dataType,dsj->dataObject[0]->data,ptrj);
            
            if(inputMapExpression[j]==between){
              if(pixel[j]>=inputMapExprValuesLow[j]&&pixel[j]<=inputMapExprValuesHigh[j])
                conditialMap[j]=true;else conditialMap[j]=false;
            }
            if(inputMapExpression[j]==notbetween){
              if(pixel[j]<inputMapExprValuesLow[j]||pixel[j]>inputMapExprValuesHigh[j])
                conditialMap[j]=true;else conditialMap[j]=false;
            }
            if(inputMapExpression[j]==lessthan){
              if(pixel[j]<inputMapExprValuesLow[j])
                conditialMap[j]=true;else conditialMap[j]=false;
            }
            if(inputMapExpression[j]==greaterthan){
              if(pixel[j]>inputMapExprValuesLow[j])
                conditialMap[j]=true;else conditialMap[j]=false;
            }
          }
          bool result = conditialMap[1];
          for(size_t j=2;j<dataSources.size();j++){
            if(combineBooleanMapExpression[j-2]==myand){
              if(result==true&&conditialMap[j]==true)result=true;else result=false;
            }
            if(combineBooleanMapExpression[j-2]==myor){
              if(result==true||conditialMap[j]==true)result=true;else result=false;
            }
          }
          if(result==true)pixel[0]=1;else pixel[0]=0;
          setValue(dataSources[0]->dataObject[0]->dataType,dataSources[0]->dataObject[0]->data,ptr,pixel[0]);
        }
      }
      CDBDebug("Warping with style %s",srvParam->Styles.c_str());
      CImageWarperRenderInterface *imageWarperRenderer;
      imageWarperRenderer = new CImgWarpNearestNeighbour();
      imageWarperRenderer->render(&imageWarper,dataSource,&drawImage);
      delete imageWarperRenderer;
      imageWarper.closereproj();
      delete [] layerStyles;
    }
    for(size_t j=0;j<dataReaders.size();j++){
      if(dataReaders[j]!=NULL){
        dataReaders[j]->close();
        delete dataReaders[j];
        dataReaders[j]=NULL;
      }
    }
    if(hasFailed==true)return 1;
    return 0;
    /**************************************************/
    
    if(status != 0)return status;
    
    if(status == 0){
      if(dataSource->cfgLayer->ImageText.size()>0){
        size_t len=strlen(dataSource->cfgLayer->ImageText[0]->value.c_str());
        drawImage.setTextStroke(dataSource->cfgLayer->ImageText[0]->value.c_str(),
                                len,
                                int(drawImage.Geo->dWidth/2-len*3),
                                drawImage.Geo->dHeight-16,240,254,-1);
      }
    }
  
  }
  return 0;
}
int CImageDataWriter::addData(std::vector <CDataSource*>&dataSources){
  
  int status;
  
  if(animation==1&&nrImagesAdded>0){
    drawImage.addImage(25);
  }
  //CDBDebug("Draw Data");
  nrImagesAdded++;
  // draw the Image
  for(size_t j=0;j<dataSources.size();j++){
    CDataSource *dataSource=dataSources[j];
    //CDBDebug("!");
    if(j!=0){if(initializeLegend(srvParam,dataSource)!=0)return 1;}
    //CDBDebug("!");
    status = warpImage(dataSource,&drawImage);
    //CDBDebug("!");
    if(status != 0)return status;
    if(j==dataSources.size()-1){
      if(status == 0){
        if(dataSource->cfgLayer->ImageText.size()>0){
          size_t len=strlen(dataSource->cfgLayer->ImageText[0]->value.c_str());
          drawImage.setTextStroke(dataSource->cfgLayer->ImageText[0]->value.c_str(),
                                  len,
                                  int(drawImage.Geo->dWidth/2-len*3),
                                  drawImage.Geo->dHeight-16,240,254,-1);
        }
      }
    }
  }
  return status;
}
int CImageDataWriter::end(){
  if(mode==0){CDBError("Not initialized");return 1;}
  if(mode==2){CDBError("Already finished");return 1;}
  mode=2;
  if(requestType==REQUEST_WMS_GETFEATUREINFO){
    printf("%s%c%c\n","Content-Type:text/html",13,10);
    printf("%s<hr>",getFeatureInfoHeader.c_str());
    printf("%s",getFeatureInfoResult.c_str());
  }
  if(requestType!=REQUEST_WMS_GETMAP&&requestType!=REQUEST_WMS_GETLEGENDGRAPHIC)return 0;
  //if(CRequest::CGI==1){
  if(requestType==REQUEST_WMS_GETMAP){
//drawCascadedWMS("http://bhw222.knmi.nl:8080/cgi-bin/interpol_overlaymap.cgi?","NL_provinces,NL_seas,NL_country_borders",true);
    //http://bhlbontw.knmi.nl/rcc/download/ensembles/cgi-bin/basemaps.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&LAYERS=world_eca,country&WIDTH=1640&HEIGHT=933&SRS=EPSG:28992&BBOX=-1176662.9631120902,-386020.95362084033,1897718.0006410922,1363001.8751485008&STYLES=&FORMAT=image/png&TRANSPARENT=TRUE&

     //drawCascadedWMS("http://bhlbontw.knmi.nl/rcc/download/ensembles/cgi-bin/basemaps.cgi?","country_lines",true);
     
  }
  
  if(animation==1){
    drawImage.addImage(100);
    drawImage.endAnimation();
    return 0;
  }
  printf("%s%c%c\n","Content-Type:image/png",13,10);
  printerrorImage(&drawImage);
  if(srvParam){
    //TODO write correct format
  }
  
  for(int y=0;y<255;y++){
    for(int x=0;x<255;x++){
      //drawImage.setPixel2(x,y,x+y*256);
    }
  }
  
  
  drawImage.printImage();
  //}
  return 0;
}
float CImageDataWriter::getValueForColorIndex(CDataSource *dataSource,int index){
  if(dataSource->stretchMinMax){
    if(dataSource->statistics==NULL){
      dataSource->statistics = new CDataSource::Statistics();
      dataSource->statistics->calculate(dataSource);
    }
    float minValue=(float)dataSource->statistics->getMinimum();
    float maxValue=(float)dataSource->statistics->getMaximum();
    //maxValue+=10;
    float ls=240/(maxValue-minValue);
    float lo=-(minValue*ls);
    dataSource->legendScale=ls;
    dataSource->legendOffset=lo;
    CDBDebug("max=%f; min=%f",maxValue,minValue);
    CDBDebug("scale=%f; offset=%f",ls,lo);
  }  
  float v=index;
  v-=dataSource->legendOffset;
  v/=dataSource->legendScale;
  if(dataSource->legendLog!=0){
    v=pow(dataSource->legendLog,v);
  }
  return v;
}
int CImageDataWriter::getColorIndexForValue(CDataSource *dataSource,float value){
  float val=value;
  if(dataSource->legendLog!=0)val=log10(val+.000001)/log10(dataSource->legendLog);
  val*=dataSource->legendScale;
  val+=dataSource->legendOffset;
  if(val>=239)val=239;else if(val<1)val=1;
  //return int(val);
  return int(val+0.5f);
}
int CImageDataWriter::createLegend(CDataSource *dataSource,CDrawImage *drawImage){
  int legendPositiveUp = 1;
  int dH=0;
  float cbH=LEGEND_HEIGHT-13-10;
  float cbW=LEGEND_WIDTH/6;
  char szTemp[256];
  CDataReader reader;
  

  
  if(renderMethod!=contourshaded){
    CT::string cacheLocation;srvParam->getCacheDirectory(&cacheLocation);
    status = reader.open(dataSource,CNETCDFREADER_MODE_OPEN_HEADER,cacheLocation.c_str());
    for(int j=0;j<cbH;j++){
      for(int i=0;i<cbW+2;i++){
        float c=(float(cbH*legendPositiveUp-j)/cbH)*240.0f;
        drawImage->setPixelIndexed(i+1,j+7+dH,int(c+1));
      }
    }
    drawImage->rectangle(0,7+dH,(int)cbW+3,(int)cbH+7+dH,248);
    float classes=6;
    for(int j=0;j<=classes;j++){
      float c=((float(classes*legendPositiveUp-j)/classes))*(cbH);
      float v=((float(j)/classes))*(240.0f);
      drawImage->line((int)cbW-4,(int)c+7+dH,(int)cbW+6,(int)c+7+dH,248);
  
      v-=dataSource->legendOffset;
      v/=dataSource->legendScale;
      if(dataSource->legendLog!=0){
        v=pow(dataSource->legendLog,v);
      }
      snprintf(szTemp,255,"%0.2f",v);
      floatToString(szTemp,255,v);
      int l=strlen(szTemp);
      drawImage->setText(szTemp,l,(int)cbW+8,(int)c+dH,248,0);
    }
  }else{
    CT::string cacheLocation;srvParam->getCacheDirectory(&cacheLocation);
    status = reader.open(dataSource,CNETCDFREADER_MODE_OPEN_ALL,cacheLocation.c_str());

    float minValue=0;
    float maxValue=0;
    
    
    if(dataSource->stretchMinMax){
      if(dataSource->statistics==NULL){
        dataSource->statistics = new CDataSource::Statistics();
        dataSource->statistics->calculate(dataSource);
      }
      minValue=(float)dataSource->statistics->getMinimum();
      maxValue=(float)dataSource->statistics->getMaximum();
    }else{
      minValue=getValueForColorIndex(dataSource,0);
      maxValue=getValueForColorIndex(dataSource,240);
    }
  
    //Only show the classes that are in the map
    if(dataSource->stretchMinMax==false){
      if(dataSource->statistics==NULL){
        dataSource->statistics = new CDataSource::Statistics();
        dataSource->statistics->calculate(dataSource);
        minValue=(float)dataSource->statistics->getMinimum();
        maxValue=(float)dataSource->statistics->getMaximum();
      }
    }
    float legendInterval=shadeInterval;
    int numClasses=(int((maxValue-minValue)/legendInterval));
    if(!dataSource->stretchMinMax){
      while(numClasses>15){
        legendInterval*=2;//(maxValue-minValue);
        numClasses=int((maxValue-minValue)/legendInterval);
      }
    }

    float iMin = int(minValue/legendInterval)*legendInterval;//-legendInterval;
    float iMax = int(maxValue/legendInterval+1)*legendInterval;//+legendInterval*1;
    
    //Calculate new scale and offset for the new min/max:
    if(dataSource->stretchMinMax==true){
      //Calculate new scale and offset for the new min/max:
      float ls=240/((iMax-iMin));
      float lo=-(iMin*ls);
      dataSource->legendScale=ls;
      dataSource->legendOffset=lo;
    }

    floatToString(szTemp,255,iMax);
   
    
    numClasses=int((iMax-iMin)/legendInterval);
    //if(numClasses<=2)numClasses=2;
    
    //CDBDebug("numClasses = %d",numClasses);
    int classSizeY=(180/(numClasses));
    if(classSizeY>18)classSizeY=18;
    //for(float j=iMax+legendInterval;j>=iMin;j=j-legendInterval){
    int classNr=0;
    for(float j=iMin;j<iMax+legendInterval;j=j+legendInterval){
      float v=j;
      
      //int y=getColorIndexForValue(dataSource,v);
      
      
      //int y2=getColorIndexForValue(dataSource,(v+legendInterval));
      int cY= int((cbH-(classNr-5))+4);
      
      int dDistanceBetweenClasses=(classSizeY-10);
      if(dDistanceBetweenClasses<4)dDistanceBetweenClasses=0;
      if(dDistanceBetweenClasses>4)dDistanceBetweenClasses=4;
      cY-=dDistanceBetweenClasses;
      int cY2=int((cbH-(classNr+classSizeY-5))+4);
      classNr+=classSizeY;
      //cY*=numClasses;
      //cY2*=numClasses;
      if(j<iMax)
      {
        int y=getColorIndexForValue(dataSource,v);
        drawImage->rectangle(4,cY2,int(cbW)+7,cY,(y),248);
              //drawImage->line((int)5,(int)cY,(int)cbW+5,(int)cY,248);
        sprintf(szTemp,"%2.1f - %2.1f",v,v+legendInterval);
      //CT::string 
      //floatToString(szTemp,255,v);
        int l=strlen(szTemp);
        drawImage->setText(szTemp,l,(int)cbW+18,((cY+cY2)/2)-7,248,0);
      }

    }
    
    /*for(int j=0;j<240;j++){
    float v=getValueForColorIndex(dataSource,j);
 
    float c=(float(cbH*legendPositiveUp-j)/cbH)*240.0f;
    if((int(v*1000)%1000)==0){
        
    drawImage->line((int)cbW-4,(int)c+7+dH,(int)cbW+6,(int)c+7+dH,248);
    floatToString(szTemp,255,v);
    int l=strlen(szTemp);
    drawImage->setText(szTemp,l,(int)cbW+8,(int)c+dH,248,0);
  }
      
  }*/
    
  }
  

  reader.close();
   //Get units
  CT::string units;
  if(status==0){
    if(dataSource->dataObject[0]->units.length()>0){
      units.concat(&dataSource->dataObject[0]->units);
    }
  }
  //Print the units under the legend:
  if(units.length()>0)drawImage->setText(units.c_str(),units.length(),5,LEGEND_HEIGHT-13,248,-1);
  
  return 0;
}
