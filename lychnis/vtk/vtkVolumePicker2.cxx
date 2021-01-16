/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkVolumePicker2.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkVolumePicker2.h"
#include "vtkObjectFactory.h"

#include "vtkBox.h"
#include "vtkImageData.h"
#include "vtkVolume.h"
#include "vtkVolumeMapper.h"

#include "vtkObjectFactory.h"

#include "vtkCommand.h"
#include "vtkCompositeDataSet.h"
//#include "vtkCompositeDataSetRange.h"
#include "vtkMath.h"
#include "vtkBox.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPlaneCollection.h"
#include "vtkTransform.h"
#include "vtkDoubleArray.h"
#include "vtkPoints.h"
#include "vtkPolygon.h"
#include "vtkVoxel.h"
#include "vtkGenericCell.h"
#include "vtkPointData.h"
#include "vtkImageData.h"
#include "vtkActor.h"
#include "vtkMapper.h"
#include "vtkTexture.h"
#include "vtkVolume.h"
#include "vtkAbstractVolumeMapper.h"
#include "vtkVolumeProperty.h"
#include "vtkLODProp3D.h"
#include "vtkImageMapper3D.h"
#include "vtkRenderer.h"
#include "vtkCamera.h"
#include "vtkAbstractCellLocator.h"
//#include "vtkUniformHyperTreeGrid.h"
//#include "vtkAbstractHyperTreeGridMapper.h"
#include "vtkBitArray.h"
//#include "vtkHyperTreeGridNonOrientedGeometryCursor.h"
#include <vtkRenderWindow.h>
#include <vtkAssemblyPath.h>
#include <vtkProperty.h>
#include <vtkImageSlice.h>
#include <vtkProp3DCollection.h>

#include <algorithm>

#include <QDebug>

vtkStandardNewMacro(vtkVolumePicker2);

//----------------------------------------------------------------------------
// Intersect a vtkVolume with a line by ray casting.  Compared to the
// same method in the superclass, this method will look for cropping planes.

#define VTKCELLPICKER_PLANE_TOL 1e-14

int vtkVolumePicker2::Pick3DInternal(vtkRenderer *renderer, double p1World[4], double p2World[4])
{
  int i;
  vtkProp *prop;
  vtkAbstractMapper3D *mapper = nullptr;
  double p1Mapper[4], p2Mapper[4];
  int winSize[2] = {1, 1};
  double x, y, t;
  double *viewport;
  double ray[3];
  int pickable;
  int LODId;
  double windowLowerLeft[4], windowUpperRight[4];
  double bounds[6], tol;
  double hitPosition[3];

  bounds[0] = bounds[1] = bounds[2] = bounds[3] = bounds[4] = bounds[5] = 0;


  if ( renderer == nullptr )
  {
    vtkErrorMacro(<<"Must specify renderer!");
    return 0;
  }

  // Compute the tolerance in world coordinates.  Do this by
  // determining the world coordinates of the diagonal points of the
  // window, computing the width of the window in world coordinates, and
  // multiplying by the tolerance.
  //
  renderer->SetWorldPoint(
    0.5*(p1World[0] + p2World[0]),
    0.5*(p1World[1] + p2World[1]),
    0.5*(p1World[2] + p2World[2]),
    1.0);
  renderer->WorldToDisplay();
  double *displayCoords = renderer->GetDisplayPoint();
  double tolZ = displayCoords[2];

  viewport = renderer->GetViewport();
  if (renderer->GetRenderWindow())
  {
    int *winSizePtr = renderer->GetRenderWindow()->GetSize();
    if (winSizePtr)
    {
      winSize[0] = winSizePtr[0];
      winSize[1] = winSizePtr[1];
    }
  }
  x = winSize[0] * viewport[0];
  y = winSize[1] * viewport[1];
  renderer->SetDisplayPoint(x, y, tolZ);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(windowLowerLeft);

  x = winSize[0] * viewport[2];
  y = winSize[1] * viewport[3];
  renderer->SetDisplayPoint(x, y, tolZ);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(windowUpperRight);

  for (tol=0.0,i=0; i<3; i++)
  {
    tol += (windowUpperRight[i] - windowLowerLeft[i]) *
      (windowUpperRight[i] - windowLowerLeft[i]);
  }

  tol = sqrt (tol) * this->Tolerance;

  //  Loop over all props.  Transform ray (defined from position of
  //  camera to selection point) into coordinates of mapper (not
  //  transformed to actors coordinates!  Reduces overall computation!!!).
  //  Note that only vtkProp3D's can be picked by vtkPicker.
  //
  vtkPropCollection *props;
  vtkProp *propCandidate;
  if ( this->PickFromList )
  {
    props = this->GetPickList();
  }
  else
  {
    props = renderer->GetViewProps();
  }

  vtkActor *actor;
  vtkLODProp3D *prop3D;
  vtkVolume *volume;
  vtkImageSlice *imageSlice = nullptr;
  vtkAssemblyPath *path;
  vtkProperty *tempProperty;
  this->Transform->PostMultiply();
  vtkCollectionSimpleIterator pit;
  double scale[3];

  m_maxIntensity=-1;m_bMaxIntensity=false;//double targetPoint[3];bool bTargeted=false;
  for ( props->InitTraversal(pit); (prop=props->GetNextProp(pit)); )
  {
    for ( prop->InitPathTraversal(); (path=prop->GetNextPath()); )
    {
      pickable = 0;m_bMaxIntensity=false;
      actor = nullptr;
      propCandidate = path->GetLastNode()->GetViewProp();
      if ( propCandidate->GetPickable() && propCandidate->GetVisibility() )
      {
        pickable = 1;
        if ( (actor=vtkActor::SafeDownCast(propCandidate)) != nullptr )
        {
          mapper = actor->GetMapper();
          if ( actor->GetProperty()->GetOpacity() <= 0.0 )
          {
            pickable = 0;
          }
        }
        else if ( (prop3D=vtkLODProp3D::SafeDownCast(propCandidate)) != nullptr )
        {
          LODId = prop3D->GetPickLODID();
          mapper = prop3D->GetLODMapper(LODId);

          // if the mapper is a vtkMapper (as opposed to a vtkVolumeMapper),
          // then check the transparency to see if the object is pickable
          if ( vtkMapper::SafeDownCast(mapper) != nullptr)
          {
            prop3D->GetLODProperty(LODId, &tempProperty);
            if ( tempProperty->GetOpacity() <= 0.0 )
            {
              pickable = 0;
            }
          }
        }
        else if ( (volume=vtkVolume::SafeDownCast(propCandidate)) != nullptr )
        {
          mapper = volume->GetMapper();
        }
        else if ( (imageSlice=vtkImageSlice::SafeDownCast(propCandidate)) )
        {
          mapper = imageSlice->GetMapper();
        }
        else
        {
          pickable = 0; //only vtkProp3D's (actors and volumes) can be picked
        }
      }

      //  If actor can be picked, get its composite matrix, invert it, and
      //  use the inverted matrix to transform the ray points into mapper
      //  coordinates.
      if ( pickable )
      {
        vtkMatrix4x4 *lastMatrix = path->GetLastNode()->GetMatrix();
        if (lastMatrix == nullptr)
        {
          vtkErrorMacro (<< "Pick: Null matrix.");
          return 0;
        }
        this->Transform->SetMatrix(lastMatrix);
        this->Transform->Push();
        this->Transform->Inverse();
        this->Transform->GetScale(scale); //need to scale the tolerance

        this->Transform->TransformPoint(p1World,p1Mapper);
        this->Transform->TransformPoint(p2World,p2Mapper);

        for (i=0; i<3; i++)
        {
          ray[i] = p2Mapper[i] - p1Mapper[i];
        }

        this->Transform->Pop();

        //  Have the ray endpoints in mapper space, now need to compare this
        //  with the mapper bounds to see whether intersection is possible.
        //
        //  Get the bounding box of the modeller.  Note that the tolerance is
        //  added to the bounding box to make sure things on the edge of the
        //  bounding box are picked correctly.
        if ( mapper != nullptr )
        {
          mapper->GetBounds(bounds);
        }

        bounds[0] -= tol; bounds[1] += tol;
        bounds[2] -= tol; bounds[3] += tol;
        bounds[4] -= tol; bounds[5] += tol;

        if ( vtkBox::IntersectBox(bounds, p1Mapper, ray, hitPosition, t) )
        {
          t = this->IntersectWithLine(
            p1Mapper, p2Mapper, tol*0.333*(scale[0]+scale[1]+scale[2]),
            path, static_cast<vtkProp3D *>(propCandidate), mapper);

          /*if ( t < VTK_DOUBLE_MAX )
          {
            //double p[3];
              if(m_bMaxIntensity){
                  targetPoint[0] = (1.0 - t)*p1World[0] + t*p2World[0];
                  targetPoint[1] = (1.0 - t)*p1World[1] + t*p2World[1];
                  targetPoint[2] = (1.0 - t)*p1World[2] + t*p2World[2];
                  bTargeted=true;m_bMaxIntensity=false;
              }

            // The IsItemPresent method returns "index+1"
            // int prevIndex = this->Prop3Ds->IsItemPresent(prop)-1;

            if (prevIndex >= 0)
            {
              // If already in list, set point to the closest point
              double oldp[3];
              this->PickedPositions->GetPoint(prevIndex, oldp);qDebug()<<m_bMaxIntensity;
              //if (vtkMath::Distance2BetweenPoints(p1World, p) < vtkMath::Distance2BetweenPoints(p1World, oldp))
              if(m_bMaxIntensity)
              {
                this->PickedPositions->SetPoint(prevIndex, p);
              }
            }
            else
            qDebug()<<m_bMaxIntensity<<m_maxIntensity;
            if (m_bMaxIntensity)
            {
              //this->Prop3Ds->AddItem(static_cast<vtkProp3D *>(prop));

              this->PickedPositions->InsertNextPoint(p);

              // backwards compatibility: also add to this->Actors
              if (actor)
              {
                //this->Actors->AddItem(actor);
              }
            }
          }*/
        }
      }//if visible and pickable and not transparent
    }//for all parts
  }//for all actors

  //if(bTargeted){this->PickedPositions->InsertNextPoint(targetPoint);}

  int picked = 0;

  if (this->Path)
  {
    // Invoke pick method if one defined - prop goes first
    this->Path->GetFirstNode()->GetViewProp()->Pick();
    this->InvokeEvent(vtkCommand::PickEvent,nullptr);
    picked = 1;
  }

  return picked;
}

double vtkVolumePicker2::IntersectWithLine(const double p1[3], const double p2[3],
                                          double tol,
                                          vtkAssemblyPath *path,
                                          vtkProp3D *prop,
                                          vtkAbstractMapper3D *m)
{
  vtkMapper *mapper = nullptr;
  vtkAbstractVolumeMapper *volumeMapper = nullptr;
  vtkImageMapper3D *imageMapper = nullptr;

  double tMin = VTK_DOUBLE_MAX;
  double t1 = 0.0;
  double t2 = 1.0;

  // Clip the ray with the mapper's ClippingPlanes and adjust t1, t2.
  // This limits the pick search to the inside of the clipped region.
  int clippingPlaneId = -1;
  if (m && !this->ClipLineWithPlanes(m, this->Transform->GetMatrix(),
                                     p1, p2, t1, t2, clippingPlaneId))
  {
    return VTK_DOUBLE_MAX;
  }

  // Initialize the pick position to the frontmost clipping plane
  if (this->PickClippingPlanes && clippingPlaneId >= 0)
  {
    tMin = t1;
  }

  // Volume
  else if ( (volumeMapper = vtkAbstractVolumeMapper::SafeDownCast(m)) )
  {
    tMin = this->IntersectVolumeWithLine(p1, p2, t1, t2, prop, volumeMapper);
  }

  // Image
  else if ( (imageMapper = vtkImageMapper3D::SafeDownCast(m)) )
  {
    tMin = this->IntersectImageWithLine(p1, p2, t1, t2, prop, imageMapper);
  }

  // Actor
  else if ( (mapper = vtkMapper::SafeDownCast(m)) )
  {
    tMin = this->IntersectActorWithLine(p1, p2, t1, t2, tol, prop, mapper);
  }

  // Unidentified Prop3D
  else
  {
    tMin = this->IntersectProp3DWithLine(p1, p2, t1, t2, tol, prop, m);
  }

  if (m_bMaxIntensity)//tMin < this->GlobalTMin)
  {
    this->GlobalTMin = tMin;
    this->SetPath(path);

    this->ClippingPlaneId = clippingPlaneId;

    // If tMin == t1, the pick didn't go past the first clipping plane,
    // so the position and normal will be set from the clipping plane.
    if (fabs(tMin - t1) < VTKCELLPICKER_PLANE_TOL && clippingPlaneId >= 0)
    {
      this->MapperPosition[0] = p1[0]*(1.0-t1) + p2[0]*t1;
      this->MapperPosition[1] = p1[1]*(1.0-t1) + p2[1]*t1;
      this->MapperPosition[2] = p1[2]*(1.0-t1) + p2[2]*t1;

      double plane[4];
      m->GetClippingPlaneInDataCoords(
        this->Transform->GetMatrix(), clippingPlaneId, plane);
      vtkMath::Normalize(plane);
      // Want normal outward from the planes, not inward
      this->MapperNormal[0] = -plane[0];
      this->MapperNormal[1] = -plane[1];
      this->MapperNormal[2] = -plane[2];
    }

    // The position comes from the data, so put it into world coordinates
    this->Transform->TransformPoint(this->MapperPosition, this->PickPosition);
    this->Transform->TransformNormal(this->MapperNormal, this->PickNormal);
  }

  return tMin;
}

double vtkVolumePicker2::IntersectVolumeWithLine(const double p1[3],
                                                const double p2[3],
                                                double t1, double t2,
                                                vtkProp3D *prop,
                                                vtkAbstractVolumeMapper *mapper)
{
  double tMin = VTK_DOUBLE_MAX;

  vtkImageData *data = vtkImageData::SafeDownCast(mapper->GetDataSetInput());
  vtkVolumeMapper *vmapper = vtkVolumeMapper::SafeDownCast(mapper);

  if (data == nullptr)
  {
    // This picker only works with image inputs
    return VTK_DOUBLE_MAX;
  }

  // Convert ray to structured coordinates
  double spacing[3], origin[3];
  int extent[6];
  data->GetSpacing(spacing);
  data->GetOrigin(origin);
  data->GetExtent(extent);

  double x1[3], x2[3];
  for (int i = 0; i < 3; i++)
  {
    x1[i] = (p1[i] - origin[i])/spacing[i];
    x2[i] = (p2[i] - origin[i])/spacing[i];
  }

  // These are set to the plane that the ray enters through
  int planeId = -1;
  int extentPlaneId = -1;

  // There might be multiple regions, depending on cropping flags
  int numSegments = 1;
  double t1List[16], t2List[16], s1List[16];
  int planeIdList[16];
  t1List[0] = t1;
  t2List[0] = t2;
  // s1 is the cropping plane intersection, initialize to large value
  double s1 = s1List[0] = VTK_DOUBLE_MAX;
  planeIdList[0] = -1;

  // Find the cropping bounds in structured coordinates
  double bounds[6];
  for (int j = 0; j < 6; j++)
  {
    bounds[j] = extent[j];
  }

  if (false)//vmapper && vmapper->GetCropping())
  {
    /*vmapper->GetCroppingRegionPlanes(bounds);
    for (int j = 0; j < 3; j++)
    {
      double b1 = (bounds[2*j] - origin[j])/spacing[j];
      double b2 = (bounds[2*j+1] - origin[j])/spacing[j];
      bounds[2*j] = (b1 < b2 ? b1 : b2);
      bounds[2*j+1] = (b1 < b2 ? b2 : b1);
      if (bounds[2*j] < extent[2*j]) { bounds[2*j] = extent[2*j]; }
      if (bounds[2*j+1] > extent[2*j+1]) { bounds[2*j+1] = extent[2*j+1]; }
      if (bounds[2*j] > bounds[2*j+1])
      {
        return VTK_DOUBLE_MAX;
      }
    }

    // Get all of the line segments that intersect the visible blocks
    int flags = vmapper->GetCroppingRegionFlags();
    if (!this->ClipLineWithCroppingRegion(bounds, extent, flags, x1, x2,
                                          t1, t2, extentPlaneId, numSegments,
                                          t1List, t2List, s1List, planeIdList))
    {
      return VTK_DOUBLE_MAX;
    }*/
  }
  else
  {
    // If no cropping, then use volume bounds
    double s2;
    if (!this->ClipLineWithExtent(extent, x1, x2, s1, s2, extentPlaneId))
    {
      return VTK_DOUBLE_MAX;
    }
    s1List[0] = s1;
    t1List[0] = ( (s1 > t1) ? s1 : t1 );
    t2List[0] = ( (s2 < t2) ? s2 : t2 );
  }

  if (false)//this->PickCroppingPlanes && vmapper && vmapper->GetCropping())
  {
    // Only require information about the first intersection
    /*s1 = s1List[0];
    if (s1 > t1)
    {
      planeId = planeIdList[0];
    }

    // Set data values at the intersected cropping or clipping plane
    if ((tMin = t1List[0]) < this->GlobalTMin)
    {
      this->ResetPickInfo();
      this->DataSet = data;
      this->Mapper = vmapper;

      double x[3];
      for (int j = 0; j < 3; j++)
      {
        x[j] = x1[j]*(1.0 - tMin) + x2[j]*tMin;
        if (planeId >= 0 && j == planeId/2)
        {
          x[j] = bounds[planeId];
        }
        else if (planeId < 0 && extentPlaneId >= 0 && j == extentPlaneId/2)
        {
          x[j] = extent[extentPlaneId];
        }
        this->MapperPosition[j] = x[j]*spacing[j] + origin[j];
      }

      this->SetImageDataPickInfo(x, extent);
    }*/
  }
  else
  {
    // Go through the segments in order, until a hit occurs
    for (int segment = 0; segment < numSegments; segment++)
    {
      if ((tMin = IntersectVolumeWithLine2(
           p1, p2, t1List[segment], t2List[segment], prop, mapper))
           < VTK_DOUBLE_MAX)
      {
        s1 = s1List[segment];
        // Keep the first planeId that was set at the first intersection
        // that occurred after t1
        if (planeId < 0 && s1 > t1)
        {
          planeId = planeIdList[segment];
        }
        break;
      }
    }
  }

  if (tMin < this->GlobalTMin)
  {
    this->CroppingPlaneId = planeId;
    // If t1 is at a cropping or extent plane, use the plane normal
    if (planeId < 0)
    {
      planeId = extentPlaneId;
    }
    if (planeId >= 0 && tMin == s1)
    {
      this->MapperNormal[0] = 0.0;
      this->MapperNormal[1] = 0.0;
      this->MapperNormal[2] = 0.0;
      this->MapperNormal[planeId/2] = 2.0*(planeId%2) - 1.0;
      if (spacing[planeId/2] < 0)
      {
        this->MapperNormal[planeId/2] = - this->MapperNormal[planeId/2];
      }
    }
  }

  return tMin;
}

#define VTKCELLPICKER_VOXEL_TOL 1e-6

double vtkVolumePicker2::IntersectVolumeWithLine2(const double p1[3],
                                                const double p2[3],
                                                double t1, double t2,
                                                vtkProp3D *prop,
                                                vtkAbstractVolumeMapper *mapper)
{
  vtkImageData *data = vtkImageData::SafeDownCast(mapper->GetDataSetInput());

  if (data == nullptr)
  {
    // This picker only works with image inputs
    return VTK_DOUBLE_MAX;
  }

  // Convert ray to structured coordinates
  double spacing[3], origin[3];
  int extent[6];
  data->GetSpacing(spacing);
  data->GetOrigin(origin);
  data->GetExtent(extent);

  double x1[3], x2[3];
  for (int i = 0; i < 3; i++)
  {
    x1[i] = (p1[i] - origin[i])/spacing[i];
    x2[i] = (p2[i] - origin[i])/spacing[i];
  }

  // Clip the ray with the extent, results go in s1 and s2
  int planeId;
  double s1, s2;
  if (!this->ClipLineWithExtent(extent, x1, x2, s1, s2, planeId))
  {
    return VTK_DOUBLE_MAX;
  }
  if (s1 >= t1) { t1 = s1; }
  if (s2 <= t2) { t2 = s2; }

  // Sanity check
  if (t2 < t1)
  {
    return VTK_DOUBLE_MAX;
  }

  // Get the property from the volume or the LOD
  vtkVolumeProperty *property = nullptr;
  vtkVolume *volume = nullptr;
  vtkLODProp3D *lodVolume = nullptr;
  if ( (volume = vtkVolume::SafeDownCast(prop)) )
  {
    property = volume->GetProperty();
  }
  else if ( (lodVolume = vtkLODProp3D::SafeDownCast(prop)) )
  {
    int lodId = lodVolume->GetPickLODID();
    lodVolume->GetLODProperty(lodId, &property);
  }

  // Get the threshold for the opacity
  //double opacityThreshold = this->VolumeOpacityIsovalue;

  // Compute the length of the line intersecting the volume
  double rayLength = sqrt(vtkMath::Distance2BetweenPoints(x1, x2))*(t2 - t1);

  // This is the minimum increment that will be allowed
  double tTol = VTKCELLPICKER_VOXEL_TOL/rayLength*(t2 - t1);

  // Find out whether there are multiple components in the volume
  int numComponents = data->GetNumberOfScalarComponents();
  int independentComponents = 0;
  if (property)
  {
    independentComponents = property->GetIndependentComponents();
  }
  int numIndependentComponents = 1;
  if (independentComponents)
  {
    numIndependentComponents = numComponents;
  }

  // Create a scalar array, it will be needed later
  vtkDataArray *scalars = vtkDataArray::CreateDataArray(data->GetScalarType());
  scalars->SetNumberOfComponents(numComponents);
  vtkIdType scalarArraySize = numComponents*data->GetNumberOfPoints();
  int scalarSize = data->GetScalarSize();
  void *scalarPtr = data->GetScalarPointer();

  // Go through each volume component separately
  double tMin = VTK_DOUBLE_MAX;
  for (int component = 0; component < numIndependentComponents; component++)
  {
    vtkPiecewiseFunction *scalarOpacity =
      (property ? property->GetScalarOpacity(component) : nullptr);
    int disableGradientOpacity =
      (property ? property->GetDisableGradientOpacity(component) : 1);
    vtkPiecewiseFunction *gradientOpacity = nullptr;
    if (!disableGradientOpacity && this->UseVolumeGradientOpacity)
    {
      gradientOpacity = property->GetGradientOpacity(component);
    }

    // This is the component used to compute the opacity
    int oComponent = component;
    if (!independentComponents)
    {
      oComponent = numComponents - 1;
    }

    // Make a new array, shifted to the desired component
    scalars->SetVoidArray(static_cast<void *>(static_cast<char *>(scalarPtr)
                                              + scalarSize*oComponent),
                          scalarArraySize, 1);

    // Do a ray cast with linear interpolation.
    double opacity = 0.0;
    double lastOpacity = 0.0,maxOpacity=0;
    double lastT = t1,maxT=t1;
    double x[3],maxX[3];
    double pcoords[3];
    int xi[3];

    // Ray cast loop
    double t = t1;//t2=1;qDebug()<<t1<<t2;
    while (t <= t2)
    {
      for (int j = 0; j < 3; j++)
      {
        // "t" is the fractional distance between endpoints x1 and x2
        x[j] = x1[j]*(1.0 - t) + x2[j]*t;

        // Paranoia bounds check
        if (x[j] < extent[2*j]) { x[j] = extent[2*j]; }
        else if (x[j] > extent[2*j + 1]) { x[j] = extent[2*j+1]; }

        xi[j] = vtkMath::Floor(x[j]);
        pcoords[j] = x[j] - xi[j];
      }

      opacity = this->ComputeVolumeOpacity2(xi, pcoords, data, scalars,
                                           scalarOpacity, gradientOpacity);

      if(opacity>maxOpacity){
          maxOpacity=opacity;maxT=t;for(int s=0;s<3;s++){maxX[s]=x[s];}
      }

      // If the ray has crossed the isosurface, then terminate the loop
      //if (opacity > opacityThreshold){break;}

      lastT = t;
      lastOpacity = opacity;

      // Compute the next "t" value that crosses a voxel boundary
      t = 1.0;
      for (int k = 0; k < 3; k++)
      {
        // Skip dimension "k" if it is perpendicular to ray
        if (fabs(x2[k] - x1[k]) > VTKCELLPICKER_VOXEL_TOL*rayLength)
        {
          // Compute the previous coord along dimension "k"
          double lastX = x1[k]*(1.0 - lastT) + x2[k]*lastT;

          // Increment to next slice boundary along dimension "k",
          // including a tolerance value for stability in cases
          // where lastX is just less than an integer value.
          double nextX = 0;
          if (x2[k] > x1[k])
          {
            nextX = vtkMath::Floor(lastX + VTKCELLPICKER_VOXEL_TOL) + 1;
          }
          else
          {
            nextX = vtkMath::Ceil(lastX - VTKCELLPICKER_VOXEL_TOL) - 1;
          }

          // Compute the "t" value for this slice boundary
          double ttry = lastT + (nextX - lastX)/(x2[k] - x1[k]);
          if (ttry > lastT + tTol && ttry < t)
          {
            t = ttry;
          }
        }
      }

      // Break if far clipping plane has been reached
      if (t >= 1.0)
      {
        t = 1.0;
        break;
      }
    } // End of "while (t <= t2)"

    // If the ray hit the isosurface, compute the isosurface position
    if (maxOpacity>0)//true){//opacity > opacityThreshold)
    {
        m_bMaxIntensity=(maxOpacity>m_maxIntensity);
        if(m_bMaxIntensity){m_maxIntensity=maxOpacity;}

            opacity=maxOpacity;t=maxT;
            for(int s=0;s<3;s++){x[s]=maxX[s];}
      // Backtrack to the actual surface position unless this was first step
      /*if (t > t1)
      {
        double f = (opacityThreshold - lastOpacity)/(opacity - lastOpacity);t = lastT*(1.0 - f) + t*f;
        for (int j = 0; j < 3; j++)
        {
          x[j] = x1[j]*(1.0 - t) + x2[j]*t;
          if (x[j] < extent[2*j]) { x[j] = extent[2*j]; }
          else if (x[j] > extent[2*j + 1]) { x[j] = extent[2*j+1]; }
          xi[j] = vtkMath::Floor(x[j]);
          pcoords[j] = x[j] - xi[j];
        }
      }*/

      // Check to see if this is the new global minimum
      if (true)//{//t < tMin && t < this->GlobalTMin)
      {
        this->ResetPickInfo();
        tMin = t;

        this->Mapper = mapper;
        this->DataSet = data;

        this->SetImageDataPickInfo(x, extent);

        this->MapperPosition[0] = x[0]*spacing[0] + origin[0];
        this->MapperPosition[1] = x[1]*spacing[1] + origin[1];
        this->MapperPosition[2] = x[2]*spacing[2] + origin[2];

        // Default the normal to the view-plane normal.  This default
        // will be used if the gradient cannot be computed any other way.
        this->MapperNormal[0] = p1[0] - p2[0];
        this->MapperNormal[1] = p1[1] - p2[1];
        this->MapperNormal[2] = p1[2] - p2[2];
        vtkMath::Normalize(this->MapperNormal);

        // Check to see if this is the first step, which means that this
        // is the boundary of the volume.  If this is the case, use the
        // normal of the boundary.
        if (t == t1 && planeId >= 0 && xi[planeId/2] == extent[planeId])
        {
          this->MapperNormal[0] = 0.0;
          this->MapperNormal[1] = 0.0;
          this->MapperNormal[2] = 0.0;
          this->MapperNormal[planeId/2] = 2.0*(planeId%2) - 1.0;
          if (spacing[planeId/2] < 0)
          {
            this->MapperNormal[planeId/2] = - this->MapperNormal[planeId/2];
          }
        }
        /*else
        {
          // Set the normal from the direction of the gradient
          int *ci = this->CellIJK;
          double weights[8];
          vtkVoxel::InterpolationFunctions(this->PCoords, weights);
          data->GetVoxelGradient(ci[0], ci[1], ci[2], scalars,
                                 this->Gradients);
          double v[3]; v[0] = v[1] = v[2] = 0.0;
          for (int k = 0; k < 8; k++)
          {
            double *pg = this->Gradients->GetTuple(k);
            v[0] += pg[0]*weights[k];
            v[1] += pg[1]*weights[k];
            v[2] += pg[2]*weights[k];
          }

          double norm = vtkMath::Norm(v);
          if (norm > 0)
          {
            this->MapperNormal[0] = v[0]/norm;
            this->MapperNormal[1] = v[1]/norm;
            this->MapperNormal[2] = v[2]/norm;
          }
        }*/
      }// End of "if (opacity > opacityThreshold)"
    } // End of "if (t < tMin && t < this->GlobalTMin)"
  } // End of loop over volume components

  scalars->Delete();

  return tMin;
}

double vtkVolumePicker2::ComputeVolumeOpacity2(
  const int xi[3], const double pcoords[3],
  vtkImageData *data, vtkDataArray *scalars,
  vtkPiecewiseFunction *scalarOpacity, vtkPiecewiseFunction *gradientOpacity)
{
  double opacity = 1.0;

  // Get interpolation weights from the pcoords
  double weights[8];
  vtkVoxel::InterpolationFunctions(const_cast<double *>(pcoords), weights);

  // Get the volume extent to avoid out-of-bounds
  int extent[6];
  data->GetExtent(extent);
  int scalarType = data->GetScalarType();

  // Compute the increments for the three directions, checking the bounds
  vtkIdType xInc = 1;
  vtkIdType yInc = extent[1] - extent[0] + 1;
  vtkIdType zInc = yInc*(extent[3] - extent[2] + 1);
  if (xi[0] == extent[1]) { xInc = 0; }
  if (xi[1] == extent[3]) { yInc = 0; }
  if (xi[2] == extent[5]) { zInc = 0; }

  // Use the increments and weights to interpolate the data
  vtkIdType ptId = data->ComputePointId(const_cast<int *>(xi));
  double val = 0.0;
  for (int j = 0; j < 8; j++)
  {
    vtkIdType ptInc = (j & 1)*xInc + ((j>>1) & 1)*yInc + ((j>>2) & 1)*zInc;
    val += weights[j]*scalars->GetComponent(ptId + ptInc, 0);
  }

  return val/255.0;

  // Compute the ScalarOpacity
  if (scalarOpacity)
  {
    opacity *= scalarOpacity->GetValue(val);
  }
  else if (scalarType == VTK_FLOAT || scalarType == VTK_DOUBLE)
  {
    opacity *= val;
  }
  else
  {
    // Assume unsigned char
    opacity *= val/255.0;
  }

  // Compute gradient and GradientOpacity
  /*if (gradientOpacity)
  {
    data->GetVoxelGradient(xi[0], xi[1], xi[2], scalars, this->Gradients);
    double v[3]; v[0] = v[1] = v[2] = 0.0;
    for (int k = 0; k < 8; k++)
    {
      double *pg = this->Gradients->GetTuple(k);
      v[0] += pg[0]*weights[k];
      v[1] += pg[1]*weights[k];
      v[2] += pg[2]*weights[k];
    }
    double grad = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    opacity *= gradientOpacity->GetValue(grad);
  }*/

  return opacity;
}
