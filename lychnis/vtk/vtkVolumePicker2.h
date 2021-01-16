#ifndef vtkVolumePicker2_h
#define vtkVolumePicker2_h

#include "vtkRenderingVolumeModule.h" // For export macro
#include "vtkVolumePicker.h"

class vtkVolumePicker2 : public vtkVolumePicker
{
    double m_maxIntensity;bool m_bMaxIntensity;
public:
  static vtkVolumePicker2 *New();
  vtkTypeMacro(vtkVolumePicker2, vtkCellPicker);
protected:
  virtual int Pick3DInternal(vtkRenderer *ren, double p1World[4], double p2World[4]) override;

  double IntersectWithLine(const double p1[3], const double p2[3], double tol,
                                    vtkAssemblyPath *path, vtkProp3D *p,
                                    vtkAbstractMapper3D *m) override;

  double IntersectVolumeWithLine(const double p1[3],
                                         const double p2[3],
                                         double t1, double t2,
                                         vtkProp3D *prop,
                                         vtkAbstractVolumeMapper *mapper) override;

  double IntersectVolumeWithLine2(const double p1[3],
                                           const double p2[3],
                                           double t1, double t2,
                                           vtkProp3D *prop,
                                           vtkAbstractVolumeMapper *mapper);

  double ComputeVolumeOpacity2(const int xi[3], const double pcoords[3],
                                vtkImageData *data, vtkDataArray *scalars,
                                vtkPiecewiseFunction *scalarOpacity,
                                vtkPiecewiseFunction *gradientOpacity);
};

#endif


