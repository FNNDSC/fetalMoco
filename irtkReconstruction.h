#ifndef _irtkReconstruction_H

#define _irtkReconstruction_H

#include <irtkImage.h>
#include <irtkTransformation.h>
#include <irtkGaussianBlurring.h>

#include <vector>
using namespace std;


/*

Reconstruction of volume from 2D slices

*/

class irtkReconstruction : public irtkObject
{

protected:

  //Structures to store the matrix of transformation between volume and slices
  struct POINT
  {
    short x;
    short y;
    short z;
    double value;
  };

  typedef std::vector<POINT> VOXELCOEFFS; 
  typedef std::vector<std::vector<VOXELCOEFFS> > SLICECOEFFS;
  std::vector<SLICECOEFFS> _volcoeffs;


  //SLICES
  /// Slices
  vector<irtkRealImage> _slices;
  /// Transformations
  vector<irtkRigidTransformation> _transformations;
  /// Indicator whether slice has an overlap with volumetric mask
  vector<bool> _slice_inside;
  
  //VOLUME
  /// Reconstructed volume
  irtkRealImage _reconstructed;
  /// Flag to say whether the template volume has been created
  bool _template_created;
  /// Volume mask
  irtkRealImage _mask;
  /// Flag to say whether we have a mask
  bool _have_mask;
  /// Weights for Gaussian reconstruction
  irtkRealImage _volume_weights;
  /// Weights for regularization
  irtkRealImage _confidence_map;
  
  //EM algorithm
  /// Variance for inlier voxel errors
  double _sigma;
  /// Proportion of inlier voxels
  double _mix;
  /// Uniform distribution for outlier voxels
  double _m;
  /// Mean for inlier slice errors
  double _mean_s;
  /// Variance for inlier slice errors
  double _sigma_s;
  /// Mean for outlier slice errors
  double _mean_s2;
  /// Variance for outlier slice errors
  double _sigma_s2;
  /// Proportion of inlier slices
  double _mix_s;
  /// Step size for likelihood calculation
  double _step;
  /// Voxel posteriors
  vector<irtkRealImage> _weights;
  ///Slice posteriors
  vector<double> _slice_weight;
   
  //Bias field
  ///Variance for bias field
  double _sigma_bias;
  /// Blurring object for bias field
  irtkGaussianBlurring<irtkRealPixel>* _gb;
  /// Slice-dependent bias fields
  vector<irtkRealImage> _bias;

  ///Slice-dependent scales
  vector<double> _scale;
  
  ///Quality factor - higher means slower and better
  double _quality_factor;
  ///Intensity min and max
  double _max_intensity;
  double _min_intensity;
  
  //Gradient descent and regulatization parameters
  ///Step for gradient descent
  double _alpha;
  ///Determine what is en edge in edge-preserving smoothing
  double _delta;
  ///Amount of smoothing
  double _lambda;
    
  //utility
  ///Debug mode
  bool _debug;

  
  //Probability density functions
  ///Zero-mean Gaussian PDF
  inline double G(double x,double s);
  ///Uniform PDF
  inline double M(double m);
   
  
public:

  ///Constructor
  irtkReconstruction();
  ///Destructor
  ~irtkReconstruction();

  ///Create zero image as a template for reconstructed volume
  double CreateTemplate(irtkRealImage stack, double resolution = 0);
  ///Remember volumetric mask and smooth it if necessary
  void SetMask(irtkRealImage * mask, double sigma);  
  ///Crop image according to the mask
  void CropImage(irtkRealImage& image, irtkRealImage& mask);
  /// Transform and resample mask to the space of the image
  void TransformMask(irtkRealImage& image, irtkRealImage& mask, irtkRigidTransformation& transformation);
  ///Calculate initial registrations
  void StackRegistrations(vector<irtkRealImage>& stacks, vector<irtkRigidTransformation>& stack_transformations, int templateNumber);
  ///Create slices from the stacks and slice-dependent transformations from stack transformations
  void CreateSlicesAndTransformations(vector<irtkRealImage>& stacks, vector<irtkRigidTransformation>& stack_transformations, vector<double>& thickness);
  ///Invert all stack transformation
  void InvertStackTransformations(vector<irtkRigidTransformation>& stack_transformations);
  ///Match stack intensities
  void MatchStackIntensities(vector<irtkRealImage>& stacks,vector<irtkRigidTransformation>& stack_transformations, double averageValue);
  ///Fill image with given value
  void ClearImage(irtkRealImage& image,double value);
  ///Mask all slices
  void MaskSlices();
  ///Calculate transformation matrix between slices and voxels
  void CoeffInit();
  ///Reconstruction using weighted Gaussian PSF
  void GaussianReconstruction();
  ///Initialise variables and parameters for EM
  void InitializeEM();
  ///Initialise values of variables and parameters for EM
  void InitializeEMValues();
  ///Initalize robust statistics
  void InitializeRobustStatistics();
  ///Perform E-step 
  void EStep();
  ///Calculate slice-dependent scale
  void Scale();
  ///Calculate slice-dependent bias fields
  void Bias();
  ///Superresolution and calculation of sigma and mix
  void SuperresolutionAndMStep(int iter);
  ///Edge-preserving regularization
  void Regularization(int iter);
  ///Edge-preserving regularization with confidence map
  void AdaptiveRegularization(int iter, irtkRealImage& original);
  ///Slice to volume registrations
  void SliceToVolumeRegistration();
  ///Mask the volume
  void MaskVolume();
  ///Save slices
  void SaveSlices();
  ///Save transformations
  void SaveTransformations();
  
  ///Remember stdev for bias field
  inline void SetSigma(double sigma);
  ///Return reconstructed volume
  inline irtkRealImage GetReconstructed();
  ///Return resampled mask
  inline irtkRealImage GetMask();
  ///Set smoothing parameters
  inline void SetSmoothingParameters(double delta, double lambda);
  ///Use faster lower quality reconstruction
  inline void SpeedupOn();
  ///Use slower better quality reconstruction
  inline void SpeedupOff();
   
  //utility
  ///Save intermediate results
  inline void DebugOn();
  ///Do not save intermediate results
  inline void DebugOff();
  
  ///Write included/excluded/outside slices
  void Evaluate(int iter);
  
};

inline double irtkReconstruction::G(double x,double s)
{
  return _step*exp(-x*x/(2*s))/(sqrt(6.28*s));
}

inline double irtkReconstruction::M(double m)
{
  return m*_step;
}

inline irtkRealImage irtkReconstruction::GetReconstructed()
{
  return _reconstructed;
}

inline irtkRealImage irtkReconstruction::GetMask()
{
  return _mask;
}

inline void irtkReconstruction::DebugOn()
{
  _debug=true;
  cout<<"Debug mode."<<endl;
}

inline void irtkReconstruction::DebugOff()
{
  _debug=false;
}

inline void irtkReconstruction::SetSigma(double sigma)
{
  _sigma_bias=sigma;
}


inline void irtkReconstruction::SpeedupOn()
{
  _quality_factor=1;
}

inline void irtkReconstruction::SpeedupOff()
{
  _quality_factor=2;
}

inline void irtkReconstruction::SetSmoothingParameters(double delta, double lambda)
{
  _delta=delta;
  _lambda=lambda*delta*delta;
  _alpha = 0.05/lambda;
  if (_alpha>1) _alpha= 1;
  cout<<"delta = "<<_delta<<" lambda = "<<lambda<<" alpha = "<<_alpha<<endl;
}


#endif
