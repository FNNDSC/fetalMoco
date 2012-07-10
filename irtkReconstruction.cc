#include <irtkReconstruction.h>
#include <irtkResampling.h>
#include <irtkRegistration.h>
#include <irtkTransformation.h>

irtkReconstruction::irtkReconstruction()
{
  _step=0.0001;
  _gb=NULL;
  _debug=false;
  _quality_factor=2;
  _sigma_bias=12;
  _sigma_s=0.025;
  _sigma_s2=0.025;
  _mix_s=0.9;
  _mix=0.9;
  _delta=1;
  _lambda=0.1;
  _alpha=(0.05/_lambda)*_delta*_delta;
  _template_created=false;
  _have_mask=false;

}


irtkReconstruction::~irtkReconstruction()
{
  if (_gb != NULL)
    delete _gb;
}

double irtkReconstruction::CreateTemplate(irtkRealImage stack, double resolution)
{
  double dx,dy,dz,d;
  
  //Get image attributes - image size and voxel size
  irtkImageAttributes attr = stack.GetImageAttributes();
  
  //enlarge stack in z-direction in case top of the head is cut off  
  attr._z +=2;
  
  //create enlarged image
  irtkRealImage enlarged(attr);
  
  //determine resolution of volume to reconstruct
  if (resolution <= 0) //resolution was not given by user - set it to min of res in x or y direction
  {
    stack.GetPixelSize(&dx,&dy,&dz);
    if ((dx<=dy)&&(dx<=dz))
      d=dx;
    else
      if (dy<=dz)
        d=dy;
      else
        d=dz;
  }
  else d=resolution;
  
  cout<<"Constructing volume with isotropic voxel size "<<d<<endl;  
  cout.flush();
  
  //resample "enlarged" to resolution "d"
  irtkImageFunction *interpolator = new irtkNearestNeighborInterpolateImageFunction;
  irtkResampling<irtkRealPixel> resampling(d, d, d);
  resampling.SetInput(&enlarged);
  resampling.SetOutput(&enlarged);
  resampling.SetInterpolator(interpolator);
  resampling.Run();
  delete interpolator;
  
  //initialize recontructed volume
  _reconstructed=enlarged;
  _template_created=true;

  //return resulting resolution of the template image
  return d;
}

void irtkReconstruction::SetMask(irtkRealImage * mask, double sigma)
{
  if (!_template_created)
  {
    cerr<<"Please create the template before setting the mask, so that the mask can be resampled to the correct dimensions."<<endl;
    exit(1);
  }
  
  _mask=_reconstructed;
  
  if(mask!=NULL)
  {
    //if sigma is nonzero first smooth the mask
    if (sigma>0)
    {
      //blur mask
      irtkGaussianBlurring<irtkRealPixel> *gb = new irtkGaussianBlurring<irtkRealPixel>(sigma); 
      gb->SetInput(mask);
      gb->SetOutput(mask);
      gb->Run();
      delete gb;
      
      //binarize mask
      irtkRealPixel* ptr = mask->GetPointerToVoxels();
      for(int i=0; i<mask->GetNumberOfVoxels(); i++)
      {
        if (*ptr>0.5) *ptr = 1;
	else *ptr=0;
	ptr++;
      }

    }
    
    //resample the mask according to the template volume using identity transformation
    irtkTransformation *transformation = new irtkRigidTransformation;
    irtkImageTransformation *imagetransformation = new irtkImageTransformation;
    irtkImageFunction *interpolator = new irtkNearestNeighborInterpolateImageFunction;
    imagetransformation->SetInput (mask, transformation);
    imagetransformation->SetOutput(&_mask);
    //target is zero image, need padding -1
    imagetransformation->PutTargetPaddingValue(-1);
    //need to fill voxels in target where there is no info from source with zeroes
    imagetransformation->PutSourcePaddingValue(0);
    imagetransformation->PutInterpolator(interpolator);
    imagetransformation->Run();
    
    delete transformation;
    delete imagetransformation;
    delete interpolator;
 
  }
  else
  {
    //fill the mask with ones
    ClearImage(_mask,1);
  }
  //set flag that mask was created
  _have_mask=true;
  
  if (_debug)
    _mask.Write("mask.nii.gz");

}

void irtkReconstruction::TransformMask(irtkRealImage& image, irtkRealImage& mask, irtkRigidTransformation& transformation)
{
    //transform mask to the space of image
    irtkImageTransformation *imagetransformation = new irtkImageTransformation;
    irtkImageFunction *interpolator = new irtkNearestNeighborInterpolateImageFunction;
    imagetransformation->SetInput (&mask, &transformation);
    irtkRealImage m = image;
    imagetransformation->SetOutput(&m);
    //target contains zeros and ones image, need padding -1
    imagetransformation->PutTargetPaddingValue(-1);
    //need to fill voxels in target where there is no info from source with zeroes
    imagetransformation->PutSourcePaddingValue(0);
    imagetransformation->PutInterpolator(interpolator);
    imagetransformation->Run();
    mask=m;
    
    delete imagetransformation;
    delete interpolator;
  
}

void irtkReconstruction::CropImage(irtkRealImage& image, irtkRealImage& mask)
{
  //Crops the image according to the mask
    
  int i,j,k;
  //ROI boundaries
  int x1, x2, y1, y2, z1, z2; 
  
  
  //Original ROI
  x1 = 0;
  y1 = 0;
  z1 = 0;
  x2 = image.GetX();
  y2 = image.GetY();
  z2 = image.GetZ();

  //upper boundary for z coordinate
  int sum=0;
  for(k=image.GetZ()-1;k>=0;k--)
  {
    sum=0;
    for(j=image.GetY()-1;j>=0;j--)
      for(i=image.GetX()-1;i>=0;i--)
        if(mask.Get(i,j,k)>0) sum++;
    if (sum>0) break;
  }
  z2=k;

  //lower boundary for z coordinate
  sum=0;
  for(k=0; k<=image.GetZ()-1;k++)
  {
    sum=0;
    for(j=image.GetY()-1;j>=0;j--)
      for(i=image.GetX()-1;i>=0;i--)
        if(mask.Get(i,j,k)>0) sum++;
    if (sum>0) break;
  }
  z1=k;

  //upper boundary for y coordinate
  sum=0;
  for(j=image.GetY()-1;j>=0;j--)
  {
    sum=0;
    for(k=image.GetZ()-1;k>=0;k--)
      for(i=image.GetX()-1;i>=0;i--)
        if(mask.Get(i,j,k)>0) sum++;
    if (sum>0) break;
  }
  y2=j;

  //lower boundary for y coordinate
  sum=0;
  for(j=0; j<=image.GetY()-1;j++)
  {
    sum=0;
    for(k=image.GetZ()-1;k>=0;k--)
      for(i=image.GetX()-1;i>=0;i--)
        if(mask.Get(i,j,k)>0) sum++;
    if (sum>0) break;
  }
  y1=j;

  //upper boundary for x coordinate
  sum=0;
  for(i=image.GetX()-1;i>=0;i--)
  {
    sum=0;
    for(k=image.GetZ()-1;k>=0;k--)
      for(j=image.GetY()-1;j>=0;j--)
        if(mask.Get(i,j,k)>0) sum++;
    if (sum>0) break;
  }
  x2=i;

  //lower boundary for x coordinate
  sum=0;
  for(i=0; i<=image.GetX()-1;i++)
  {
    sum=0;
    for(k=image.GetZ()-1;k>=0;k--)
      for(j=image.GetY()-1;j>=0;j--)
        if(mask.Get(i,j,k)>0) sum++;
    if (sum>0) break;
  }

  x1=i;

  if (_debug)
    cout<<"Region of interest is "<<x1<<" "<<y1<<" "<<z1<<" "<<x2<<" "<<y2<<" "<<z2<<endl;

  //Cut region of interest
  image = image.GetRegion(x1, y1, z1, x2, y2, z2);
}

void irtkReconstruction::StackRegistrations(vector<irtkRealImage>& stacks,vector<irtkRigidTransformation>& stack_transformations, int templateNumber)
{
  //rigid registration object
  irtkImageRigidRegistration registration;
  //buffer to create the name
  char buffer[256];
  
  //template is set as the target
  irtkGreyImage target = stacks[templateNumber];
  //target needs to be masked before registration
  if (_have_mask)
  {
    double x,y,z;
    for(int i=0; i<target.GetX(); i++)
      for(int j=0; j<target.GetY(); j++)
        for(int k=0; k<target.GetZ(); k++)
        {
          //image coordinates of the target
	  x=i;
	  y=j;
	  z=k;
	  //change to world coordinates
	  target.ImageToWorld(x,y,z);
	  //change to mask image coordinates - mask is aligned with target
	  _mask.WorldToImage(x,y,z);
	  x=round(x);
	  y=round(y);
	  z=round(z);
	  //if the voxel is outside mask ROI set it to -1 (padding value)
	  if ((x>=0)&&(x<_mask.GetX())&&(y>=0)&&(y<_mask.GetY())&&(z>=0)&&(z<_mask.GetZ()))
	  {
	    if (_mask(x,y,z)==0)
	      target(i,j,k)=0;
	  }
	  else
	    target(i,j,k)=0;	  
        }
  }

  //register all stacks to the target
  for (uint i=0; i<stacks.size(); i++)
  {
    //do not perform registration for template
    if (i == (uint)templateNumber) continue;
    
    //set target and source (need to be converted to irtkGreyImage)
    irtkGreyImage source = stacks[i];
    
    //perform rigid registration
    registration.SetInput(&target, &source);
    registration.SetOutput(&stack_transformations[i]);
    registration.GuessParameterThickSlices();
    registration.SetTargetPadding(0);
    registration.Run();

    //save volumetric registrations
    if (_debug)
    {
      registration.irtkImageRegistration::Write((char *)"parout-volume.rreg");
      sprintf(buffer,"stack-transformation%i.dof.gz",i);
      stack_transformations[i].irtkTransformation::Write(buffer);
      target.Write("target.nii.gz");
      sprintf(buffer,"stack%i.nii.gz",i);
      stacks[i].Write(buffer);
    }
  }
}

void irtkReconstruction::MatchStackIntensities(vector<irtkRealImage>& stacks,vector<irtkRigidTransformation>& stack_transformations, double averageValue)
{
  
  if (_debug)
   cout<<"Matching intensities of stacks. ";
  
  //Calculate the averages of intensities for all stacks
  vector<double> average;
  double sum,num;
  char buffer[256];
  uint ind;
  int i,j,k;
  double x,y,z;
 
  //averages need to be calculated only in ROI
  for (ind=0; ind<stacks.size(); ind++)
  {
    sum=0;
    num=0;
    for(i=0; i<stacks[ind].GetX(); i++)
      for(j=0; j<stacks[ind].GetY(); j++)
        for(k=0; k<stacks[ind].GetZ(); k++)
        {
          //image coordinates of the stack voxel
	  x=i;
	  y=j;
	  z=k;
	  //change to world coordinates
	  stacks[ind].ImageToWorld(x,y,z);
	  //transform to template (and also _mask) space
	  stack_transformations[ind].Transform(x,y,z);
	  //change to mask image coordinates - mask is aligned with template
	  _mask.WorldToImage(x,y,z);
	  x=round(x);
	  y=round(y);
	  z=round(z);
	  //if the voxel is inside mask ROI include it
	  if ((x>=0)&&(x<_mask.GetX())&&(y>=0)&&(y<_mask.GetY())&&(z>=0)&&(z<_mask.GetZ()))
	  {
	    if (_mask(x,y,z)==1)
	    {
	      sum += stacks[ind](i,j,k);
	      num++;
	    }
	  }
        }
        //calculate average for the stack
	if (num>0)
	  average.push_back(sum/num);
	else
	{
	  cerr<<"Stack "<<ind<<" has no overlap with ROI"<<endl;
	  exit(1);
	}
  }
  
  if (_debug)
  {
    cout<<"Stack average intensities are ";
    for (ind = 0; ind< average.size(); ind++)
      cout<<average[ind]<<" ";
    cout<<endl;
    cout<<"The new average value is "<<averageValue<<endl;
    cout.flush();
  }
  
  //Rescale stacks
  irtkRealPixel *ptr;
  double factor;
  for (ind=0; ind<stacks.size(); ind++)
  {
    factor = averageValue/average[ind];
    ptr = stacks[ind].GetPointerToVoxels();
    for (i=0;i<stacks[ind].GetNumberOfVoxels();i++)
    {
      if(*ptr>0)
	*ptr *= factor;
      ptr++;
    }   
  }
  
  if(_debug)
  {
    for (ind=0; ind<stacks.size(); ind++)
    {
      sprintf(buffer,"rescaled-stack%i.nii.gz",ind);
      stacks[ind].Write(buffer);
    }
  }

}

void irtkReconstruction::InvertStackTransformations(vector<irtkRigidTransformation>& stack_transformations)
{ 
  //for each stack
  for (uint i=0; i<stack_transformations.size(); i++)
  {    
    //invert transformation for the stacks
    irtkMatrix m = stack_transformations[i].GetMatrix();
    m.Invert();
    stack_transformations[i].PutMatrix(m);
  }
}

void irtkReconstruction::CreateSlicesAndTransformations(vector<irtkRealImage>& stacks, vector<irtkRigidTransformation>& stack_transformations, vector<double>& thickness)
{ 
  //for each stack
  for (uint i=0; i<stacks.size(); i++)
  {        
    //image attributes contain image and voxel size
    irtkImageAttributes attr = stacks[i].GetImageAttributes();
    
    //attr._z is number of slices in the stack
    for (int j=0; j<attr._z; j++)
    {
      //create slice by selecting the appropreate region of the stack
      irtkRealImage slice = stacks[i].GetRegion(0,0,j,attr._x,attr._y,j+1);
      //set correct voxel size in the stack. Z size is equal to slice thickness.
      slice.PutPixelSize(attr._dx,attr._dy,thickness[i]);
      //remember the slice
      _slices.push_back(slice);
      //initialize slice transformation with the stack transformation
      _transformations.push_back(stack_transformations[i]);
    }
  }
  cout<<"Number of slices: "<<_slices.size()<<endl;

}

void irtkReconstruction::ClearImage(irtkRealImage &image, double value)
{
  irtkRealPixel* ptr = image.GetPointerToVoxels();
  for (int i=0;i<image.GetNumberOfVoxels();i++)
  {
    *ptr=value;
    *ptr++;
  }
}

void irtkReconstruction::MaskSlices()
{
  cout<<"Masking slices ... ";
  
  double x,y,z;
  int i,j;
   
  //Check whether we have a mask
  if (!_have_mask)
  {
    cout<<"Could not mask slices because no mask has been set."<<endl;
    return;
  }

  //mask slices
  for (uint inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {
    irtkRealImage slice = _slices[inputIndex];
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
      {
	//image coordinates of a slice voxel
	x=i;
	y=j;
	z=0;
	//change to world coordinates in slice space
	slice.ImageToWorld(x,y,z);
	//world coordinates in volume space
	_transformations[inputIndex].Transform(x,y,z);
	//image coordinates in volume space
	_mask.WorldToImage(x,y,z);
	x=round(x);
	y=round(y);
	z=round(z);
	//if the voxel is outside mask ROI set it to -1 (padding value)
	if ((x>=0)&&(x<_mask.GetX())&&(y>=0)&&(y<_mask.GetY())&&(z>=0)&&(z<_mask.GetZ()))
	{
	  if (_mask(x,y,z)==0)
	    slice(i,j,0)=-1;
	}
	else
	  slice(i,j,0)=-1;
      }
    //remember masked slice	
    _slices[inputIndex]=slice;
  }
  cout<<"done."<<endl;
}


void irtkReconstruction::SliceToVolumeRegistration()
{
  irtkImageRigidRegistration registration;
  irtkGreyPixel smin,smax;
  irtkGreyImage target;
  irtkRealImage slice,w,b;
  uint inputIndex;

  
  for (inputIndex=0; inputIndex<_slices.size(); inputIndex++)
  {
    
    target = _slices[inputIndex];
    
    target.GetMinMax(&smin,&smax);
    if (smax>-1)
    {
      irtkGreyImage source = _reconstructed;
      registration.SetInput(&target, &source);
      registration.SetOutput(&_transformations[inputIndex]);
      registration.GuessParameterSliceToVolume();
      registration.SetTargetPadding(-1);
      if (_debug)
	 registration.irtkImageRegistration::Write((char *)"parout-slice.rreg");
      registration.Run();
    }
  }
}

void irtkReconstruction::SaveTransformations()
{
  char buffer[256];
  for (uint inputIndex=0; inputIndex<_slices.size(); inputIndex++)
  {
    sprintf(buffer,"transformation%i.dof",inputIndex);
    _transformations[inputIndex].irtkTransformation::Write(buffer);
  }
}

void irtkReconstruction::SaveSlices()
{
  char buffer[256];
  for (uint inputIndex=0; inputIndex<_slices.size(); inputIndex++)
  {
    sprintf(buffer,"slice%i.nii.gz",inputIndex);
    _slices[inputIndex].Write(buffer);
  }
}

void irtkReconstruction::CoeffInit()
{
 //clear slice-volume matrix from previous iteration
  _volcoeffs.clear();
  
  //clear indicator of slice having and overlap with volumetric mask
  _slice_inside.clear();
  
  bool slice_inside;
  
  //current slice
  irtkRealImage slice;

  //get resolution of the volume
  double vx,vy,vz;
  _reconstructed.GetPixelSize(&vx,&vy,&vz);
  //volume is always isotropic
  double res = vx;
   
  //prepare image for volume weights, will be needed for Gaussian Reconstruction
  _volume_weights =_reconstructed;
  ClearImage(_volume_weights,0);
  
  cout<<"Initialising matrix coefficients...";
  for (uint inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {
  //start of a loop for a slice inputIndex  
    cout<<inputIndex<<" ";
    cout.flush();

    //read the slice
    slice=_slices[inputIndex];  

    //prepare structures for storage  
    POINT p;
    VOXELCOEFFS empty;
    SLICECOEFFS slicecoeffs(slice.GetX(),vector<VOXELCOEFFS>(slice.GetY(),empty));

    //to check whether the slice has an overlap with mask ROI
    slice_inside = false;

     
    //PSF will be calculated in slice space in higher resolution
    
    //get slice voxel size to define PSF
    double dx,dy,dz;
    slice.GetPixelSize(&dx,&dy,&dz);
    
    //sigma of 3D Gaussian (sinc with FWHM=dx or dy in-plane, Gaussian with FWHM = dz through-plane)
    double sigmax = 1.2*dx/2.3548;
    double sigmay = 1.2*dy/2.3548;
    double sigmaz = dz/2.3548;
    
    //calculate discretized PSF
    
    //isotropic voxel size of PSF - derived from resolution of reconstructed volume
    double size = res/_quality_factor;
    
    //number of voxels in each direction
    //the ROI is 2*voxel dimension
    
    int xDim = round(2*dx/size);
    int yDim = round(2*dy/size);
    int zDim = round(2*dz/size);
    
    //image corresponding to PSF
    irtkImageAttributes attr; 
    attr._x = xDim; attr._y = yDim; attr._z = zDim;
    attr._dx = size; attr._dy = size; attr._dz = size; 
    irtkRealImage PSF(attr);
    
    //centre of PSF 
    double cx,cy,cz;
    cx=0.5*(xDim-1);
    cy=0.5*(yDim-1);
    cz=0.5*(zDim-1);
    PSF.ImageToWorld(cx,cy,cz);

    double x,y,z;
    double sum=0;
    int i,j,k;
    for (i=0; i<xDim; i++)
      for (j=0; j<yDim; j++)
        for (k=0; k<zDim; k++)
	{
	  x=i;y=j;z=k;
	  PSF.ImageToWorld(x,y,z);
	  x-=cx;y-=cy;z-=cz;
	  //continuous PSF does not need to be normalized as discreet will be
	  PSF(i,j,k) = exp(-x*x/(2*sigmax*sigmax)-y*y/(2*sigmay*sigmay)-z*z/(2*sigmaz*sigmaz));
          sum+=PSF(i,j,k);
	}
    PSF/=sum;
    
    if (_debug)
      if (inputIndex==0)
        PSF.Write("PSF.nii.gz");
    
    
    //prepare storage for PSF transformed and resampled to the space of reconstructed volume
    //maximum dim of rotated kernel - the next higher odd integer
    int dim = (floor(ceil(sqrt(xDim*xDim+yDim*yDim+zDim*zDim))/2))*2+1;
    //prepare image attributes. Voxel dimension will be taken from the reconstructed volume
    attr._x=dim; attr._y=dim;attr._z=dim;
    attr._dx=res; attr._dy=res; attr._dz=res;
    //create matrix from transformed PSF
    irtkRealImage tPSF(attr);
    //calculate centre of tPSF in image coordinates
    int centre = (dim-1)/2;

    //for each voxel in current slice calculate matrix coefficients
    int ii,jj,kk;
    int tx,ty,tz;
    int nx,ny,nz;
    int l,m,n;
    double weight;
    for(i=0;i<slice.GetX();i++)
      for(j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
        {	  
	  //calculate centrepoint of slice voxel in volume space (tx,ty,tz)
	  x=i;y=j;z=0;
	  slice.ImageToWorld(x,y,z);
	  _transformations[inputIndex].Transform(x,y,z);
	  _reconstructed.WorldToImage(x,y,z);
	  tx=round(x);ty=round(y);tz=round(z);

          //Clear the transformed PSF
	  for (ii=0; ii<dim; ii++)
            for (jj=0; jj<dim; jj++)
              for (kk=0; kk<dim; kk++)
	        tPSF(ii,jj,kk)=0;

          //for each point of the PSF
	  for (ii=0; ii<xDim; ii++)
            for (jj=0; jj<yDim; jj++)
              for (kk=0; kk<zDim; kk++)
	      {
	        //Calculate the position of the point of PSF centered over current slice voxel
		//This is a bit complicated because slices can be oriented in any direction
		
		//PSF image coordinates
	        x=ii;y=jj;z=kk;
		//change to PSF world coordinates - now real sizes in mm
	        PSF.ImageToWorld(x,y,z);
		//centre around the centrepoint of the PSF
		x-=cx; y-=cy;z-=cz;
		
		//Need to convert (x,y,z) to slice image coordinates because slices can have transformations included in them (they are nifti)  and those are not reflected in PSF. In slice image coordinates we are sure that z is through-plane 
		
		//adjust according to voxel size
		x/=dx; y/=dy;z/=dz;
		//center over current voxel
		x+=i; y+=j;
		
		//convert from slice image coordinates to world coordinates
		slice.ImageToWorld(x,y,z);
		
	        //x+=(vx-cx); y+=(vy-cy); z+=(vz-cz);
	        //Transform to space of reconstructed volume
	        _transformations[inputIndex].Transform(x,y,z);
	        //Change to image coordinates
	        _reconstructed.WorldToImage(x,y,z);
	      
	        //determine coefficients of volume voxels for position x,y,z
	        //using linear interpolation
		
		//Find the 8 closest volume voxels
		  
		//lowest corner of the cube
	        nx = (int)floor(x);
                ny = (int)floor(y);
                nz = (int)floor(z);
	          
		//not all neighbours might be in ROI, thus we need to normalize
		//(l,m,n) are image coordinates of 8 neighbours in volume space
		//for each we check whether it is in volume
		sum=0;
		//to find wether the current slice voxel has overlap with ROI
		bool inside=false;
	        for (l=nx;l<=nx+1;l++)	
		  if ((l>=0)&&(l<_reconstructed.GetX()))
	            for (m=ny;m<=ny+1;m++)	    
		      if ((m>=0)&&(m<_reconstructed.GetY()))
	                for (n=nz;n<=nz+1;n++)	    
		          if ((n>=0)&&(n<_reconstructed.GetZ()))
			    {
			      weight=(1 - fabs(l - x))*(1 - fabs(m - y))*(1 - fabs(n - z));
			      sum+=weight;
			      if (_mask(l,m,n)==1)
			      {
				inside = true;
				slice_inside = true;
			      }
			    }
		//if there were no voxels do noting
		if ((sum<=0)||(!inside)) continue;
	        //now calculate the transformed PSF
	        for (l=nx;l<=nx+1;l++)	
		  if ((l>=0)&&(l<_reconstructed.GetX()))
	            for (m=ny;m<=ny+1;m++)	    
		      if ((m>=0)&&(m<_reconstructed.GetY()))
	                for (n=nz;n<=nz+1;n++)	    
		          if ((n>=0)&&(n<_reconstructed.GetZ()))
			    {
			      weight=(1 - fabs(l - x))*(1 - fabs(m - y))*(1 - fabs(n - z));
				
		              //image coordinates in tPSF
			      //(centre,centre,centre) in tPSF is aligned with (tx,ty,tz)
			      int aa,bb,cc;
			      aa=l-tx+centre;
			      bb=m-ty+centre;
			      cc=n-tz+centre;
				
			      //resulting value
			      double value = PSF(ii,jj,kk)*weight/sum;

			      //Check that we are in tPSF
			      if ((aa<0)||(aa>=dim)||(bb<0)||(bb>=dim)||(cc<0)||(cc>=dim))
			      {
				cerr<<"Error while trying to populate tPSF. "<<aa<<" "<<bb<<" "<<cc<<endl;
				exit(1);
			      }
			      else //update transformed PSF
				tPSF(aa,bb,cc)+=value;
				
			      _volume_weights(l,m,n)+=value;
			    }
	        
	        }//end of the loop for PSF points
		
	  //store tPSF values
	  for (ii=0; ii<dim; ii++)
            for (jj=0; jj<dim; jj++)
              for (kk=0; kk<dim; kk++)
	        if (tPSF(ii,jj,kk)>0)
		{
		  p.x = ii + tx - centre;
		  p.y = jj + ty - centre;
		  p.z = kk + tz - centre;
		  p.value = tPSF(ii,jj,kk);
		  slicecoeffs[i][j].push_back(p);
		}

	  
        }//end of loop for slice voxels

     _volcoeffs.push_back(slicecoeffs);
     _slice_inside.push_back(slice_inside);

  }  //end of loop through the slices

  if (_debug)
    _volume_weights.Write("volume_weights.nii.gz");
  cout<<" ... done."<<endl;  
}//end of CoeffInit()

void irtkReconstruction::GaussianReconstruction()
{
  cout<<"Gaussian reconstruction ... ";
  uint inputIndex;
  int i,j,k,n;
  irtkRealImage slice,addon,b;
  double scale;
  POINT p;

  //clear _reconstructed image
  ClearImage(_reconstructed,0);

  for (inputIndex = 0; inputIndex < _slices.size(); ++inputIndex)
  {
    // read the current slice
    slice=_slices[inputIndex];
    //read the current bias image
    b=_bias[inputIndex];
    //read current scale factor
    scale = _scale[inputIndex];
    
    
    //Distribute slice intensities to the volume
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
	  //biascorrect and scale the slice
	  slice(i,j,0)*=exp(-b(i,j,0))*scale;
	  
	  //number of volume voxels with non-zero coefficients for current slice voxel
	  n=_volcoeffs[inputIndex][i][j].size();
	  
	  //if given voxel is not present in reconstructed volume at all pad it
	  if (n==0)
	    _slices[inputIndex].PutAsDouble(i,j,0,-1);
	  
	  //add contribution of current slice voxel to all voxel volumes
	  //to which it contributes
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    _reconstructed(p.x,p.y,p.z) += p.value*slice(i,j,0);
	  }	    
	}
   //end of loop for a slice inputIndex  
  }
  
  //normalize the volume by proportion of contributing slice voxels for each volume voxel
  for(i=0;i<_reconstructed.GetX();i++)
    for(j=0;j<_reconstructed.GetY();j++)
      for(k=0;k<_reconstructed.GetZ();k++)
        if(_volume_weights(i,j,k)>0)
	  _reconstructed(i,j,k)/=_volume_weights(i,j,k);
  cout<<"done."<<endl;
  
  if (_debug)
  _reconstructed.Write("init.nii.gz");
  
}


void irtkReconstruction::InitializeEM()
{
  //Create images for voxel weights and bias fields
  for (uint i=0; i<_slices.size(); i++)
  {
    _weights.push_back(_slices[i]);
    _bias.push_back(_slices[i]);
  }
  
  //Create and initialize scales
  for (uint i=0; i<_slices.size(); i++)
    _scale.push_back(1);

  //Create and initialize slice weights
  for (uint i=0; i<_slices.size(); i++)
    _slice_weight.push_back(1);
  
  //Initialise smoothing for bias field
   _gb = new irtkGaussianBlurring<irtkRealPixel>(_sigma_bias);   
   
  //Find the range of intensities
  _max_intensity=0;
  _min_intensity=1000000;
  for (uint i = 0; i < _slices.size(); i++)
  {
    //find min and max of slice intensities
    irtkRealPixel mmin,mmax;
    _slices[i].GetMinMax(&mmin,&mmax);
    
    //if max of the slice is bigger than last max, update
    if (mmax>_max_intensity) 
      _max_intensity=mmax;
    
    //to update minimum we need to exclude padding value
    mmin=32000;
    irtkRealPixel *ptr=_slices[i].GetPointerToVoxels();
    for(int ind=0;ind<_slices[i].GetNumberOfVoxels();ind++)
    {
      if(*ptr>0)
	if(*ptr<mmin) mmin=*ptr;
      ptr++;
    }
    //if slice min (other than padding) is smaller than last min, update
    if (mmin<_min_intensity)
      _min_intensity=mmin;
  }
}

void irtkReconstruction::InitializeEMValues()
{

  //Initialise voxel weights and bias values
  for (uint i=0; i<_slices.size(); i++)
  {
    irtkRealPixel *pw = _weights[i].GetPointerToVoxels();
    irtkRealPixel *pb = _bias[i].GetPointerToVoxels();
    irtkRealPixel *pi = _slices[i].GetPointerToVoxels();
    for (int j=0; j<_weights[i].GetNumberOfVoxels();j++)
    {
      if (*pi!=-1)
      {
        *pw=1;
        *pb=0;
      }
      else
      {
	*pw=0;
	*pb=0;
      }
      pi++;
      pw++;
      pb++;
    }
  }
  
  //Initialise slice weights
  for (uint i=0; i<_slices.size(); i++)
    _slice_weight[i]=1;

  //Initialise scaling factors for intensity matching
  for (uint i=0; i<_slices.size(); i++)
    _scale[i]=1;  
}

void irtkReconstruction::InitializeRobustStatistics()
{
  //Initialise parameter of EM robust statistics 
  int i,j,k,n;
  bool slice_inside, inside;
  irtkRealImage slice;
  POINT p;

  double sigma=0;
  int num=0;
  
  
  //for each slice
  for (uint inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {
    slice=_slices[inputIndex];

    //flag to see whether the current slice has overlap with masked ROI in volume
    slice_inside=false;
    
    //Voxel-wise sigma will be set to stdev of volumetric errors
    //For each slice voxel
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
	  //flag to see whether the current voxel is inside ROI
	  inside=false;
	  
	  n=_volcoeffs[inputIndex][i][j].size();
	  //for each volume voxel that contributes to current slice voxels
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    //contribution is subtracted to obtain the intensity difference between
	    //acquired and simulated slice
	    slice(i,j,0)-=p.value*_reconstructed(p.x,p.y,p.z);
	    if (_mask(p.x,p.y,p.z)==1)
	    {
	      slice_inside = true;
	      inside = true;
	    }
	  }
	  //calculate stev of the errors
	  if (inside)
	  {
            sigma += slice(i,j,0)*slice(i,j,0);
	    num++;
	  }
	}
    //if slice does not have an overlap with ROI, set its weight to zero	
    if (!slice_inside)
	  _slice_weight[inputIndex]=0;
  }
  
  //initialize sigma for voxelwise robust statistics
  _sigma=sigma/num;
  //initialize sigma for slice-wise robust statistics
  _sigma_s=0.025;
  //initialize mixing proportion for inlier class in voxel-wise robust statistics
  _mix=0.9;
  //initialize mixing proportion for outlier class in slice-wise robust statistics
  _mix_s=0.9;    
  //Initialise value for uniform distribution according to the range of intensities
  _m=1/(2.1*_max_intensity-1.9*_min_intensity); 
  
  if (_debug)
    cout<<"Initializing robust statistics: "<<"sigma="<<sqrt(_sigma)<<" "<<"m="<<_m<<" "<<"mix="<<_mix<<" "<<"mix_s="<<_mix_s<<endl;

}

void irtkReconstruction::EStep()
{
  //EStep performs calculation of voxel-wise and slice-wise posteriors (weights)
  if(_debug)
    cout<<"EStep: "<<endl;

  uint inputIndex;
  int i,j,k,n;
  irtkRealImage slice,w,b;
  double scale;
  POINT p;
  int num=0;
  vector<double> slice_potential;
  double g,m;
  

  //Calculate slice potentials
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {
    // read the current slice
    slice=_slices[inputIndex];
    //read current weight image
    w=_weights[inputIndex];
    //read the current bias image
    b=_bias[inputIndex];
    //identify scale factor
    scale = _scale[inputIndex];

    slice_potential.push_back(0);
    num=0;

    //Calculate error, voxel weights, and slice potential
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
  	  //bias correct and scale the slice
	  slice(i,j,0)*=exp(-b(i,j,0))*scale;
          
	  //number of volumetric voxels to which current slice voxel contributes
	  n=_volcoeffs[inputIndex][i][j].size();
	  
	  //slice voxel has no overlap with volumetric ROI, do not process it
	  if (n==0) 
	  {
	    _weights[inputIndex].PutAsDouble(i,j,0,0);
	    continue;
	  }

	  //calculate error
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    slice(i,j,0)-=p.value*_reconstructed(p.x,p.y,p.z);
	  }
	  
	  //calculate norm and voxel-wise weights
	  
	  //Gaussian distribution for inliers (likelihood)
	  g = G(slice(i,j,0),_sigma);
	  //Uniform distribution for outliers (likelihood)
	  m= M(_m);
	  
	  //voxel_wise posterior
	  double weight=g*_mix/(g*_mix+m*(1-_mix));
	  _weights[inputIndex].PutAsDouble(i,j,0,weight);
	  
	  //calculate slice potentials
          slice_potential[inputIndex]+= (1-weight)*(1-weight);
	  num++;
	}

    //evaluate slice potential
    if(num>0)
      slice_potential[inputIndex]=sqrt(slice_potential[inputIndex]/num);
    else slice_potential[inputIndex]=-1; // slice has no unpadded voxels
  }
      
  //Calulation of slice-wise robust statistics parameters.
  //This is theoretically M-step, but we want to use latest estimate of slice potentials
  //to update the parameters
  
  //Calculate means of the inlier and outlier potentials
  double sum=0, den=0, sum2=0, den2=0, maxs=0, mins=1;
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
    if (slice_potential[inputIndex]>=0)
    {
      //calculate means
      sum  += slice_potential[inputIndex]*_slice_weight[inputIndex];
      den  += _slice_weight[inputIndex];
      sum2 += slice_potential[inputIndex]*(1-_slice_weight[inputIndex]);
      den2 += (1-_slice_weight[inputIndex]);
      
      //calculate min and max of potentials in case means need to be initalized
      if (slice_potential[inputIndex]>maxs)
	maxs=slice_potential[inputIndex];
      if (slice_potential[inputIndex]<mins)
	mins=slice_potential[inputIndex];
    }
    
  if (den>0)
    _mean_s = sum/den;
  else
    _mean_s=mins;
  
  if (den2>0)
    _mean_s2 = sum2/den2;
  else
    _mean_s2=(maxs+_mean_s)/2;
  
  //Calculate the variances of the potentials
  sum=0; den=0; sum2=0; den2=0;
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
    if (slice_potential[inputIndex]>=0)
    {
      sum  += (slice_potential[inputIndex]-_mean_s)*(slice_potential[inputIndex]-_mean_s)*_slice_weight[inputIndex];
      den  += _slice_weight[inputIndex];
      
      sum2 += (slice_potential[inputIndex]-_mean_s2)*(slice_potential[inputIndex]-_mean_s2)*(1-_slice_weight[inputIndex]);
      den2 += (1-_slice_weight[inputIndex]);

    }
  
  //_sigma_s
  if ((sum>0)&&(den>0))
  {
    _sigma_s=sum/den;
    //do not allow too small sigma
    if (_sigma_s < _step*_step/6.28) 
      _sigma_s = _step*_step/6.28;      
  }
  else
  {
    _sigma_s=0.025;
    if (_debug)
    {
      if (sum <= 0) 
        cout<<"All slices are equal. ";
      if (den < 0) //this should not happen
        cout<<"All slices are outliers. ";
      cout<<"Setting sigma to "<<sqrt(_sigma_s)<<endl;
      cout.flush();
    }
  }
    
  
  //sigma_s2
  if ((sum2>0)&&(den2>0))
  {
    _sigma_s2=sum2/den2;
    //do not allow too small sigma
    if (_sigma_s2<_step*_step/6.28)
      _sigma_s2 = _step*_step/6.28;   
  }
  else
  {
    _sigma_s2=(_mean_s2-_mean_s)*(_mean_s2-_mean_s)/4;
    //do not allow too small sigma
    if (_sigma_s2<_step*_step/6.28)
      _sigma_s2 = _step*_step/6.28;   
      
    if (_debug)
    {
      if (sum2 <= 0) 
        cout<<"All slices are equal. ";
      if (den2 <= 0)  
        cout<<"All slices inliers. ";
      cout<<"Setting sigma_s2 to "<<sqrt(_sigma_s2)<<endl;
      cout.flush();
    }
  }  
  
  //Calculate slice weights  
  double gs1,gs2;  
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {  
    //Slice does not have any voxels in volumetric ROI
    if (slice_potential[inputIndex]==-1)
    {
      _slice_weight[inputIndex]=0;
      continue;
    }
    
    //All slices are outliers or the means are not valid
    if ((den<=0)||(_mean_s2 <= _mean_s))
    {
      _slice_weight[inputIndex]=1;
      continue;
    }

    //likelihood for inliers
    if (slice_potential[inputIndex] < _mean_s2)
      gs1=G(slice_potential[inputIndex]-_mean_s,_sigma_s);
    else 
      gs1=0;

    //likelihood for outliers
    if (slice_potential[inputIndex] > _mean_s)
      gs2=G(slice_potential[inputIndex]-_mean_s2,_sigma_s2);
    else
      gs2=0;

    //calculate slice weight
    double likelihood = gs1*_mix_s+gs2*(1-_mix_s);
    if (likelihood>0)
      _slice_weight[inputIndex] = gs1*_mix_s/likelihood;
    else
    {
      if (slice_potential[inputIndex] <= _mean_s)
	_slice_weight[inputIndex] = 1;
      if (slice_potential[inputIndex] >= _mean_s2)
	_slice_weight[inputIndex] = 0;
      if ((slice_potential[inputIndex] < _mean_s2) && (slice_potential[inputIndex] > _mean_s)) //should not happen
	_slice_weight[inputIndex]=1;
    }
  }

  //Update _mix_s this should also be part of MStep
  sum=0; num=0;
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
    if (slice_potential[inputIndex]>=0)
    {
      sum+=_slice_weight[inputIndex];
      num++;
    }

  if (num>0)
    _mix_s=sum/num;
  else
  {
    cout<<"All slices are outliers. Setting _mix_s to 0.9."<<endl;
    _mix_s=0.9;
  }
  
  if (_debug)
  {
    cout<<setprecision(3);
    cout<<"Slice robust statistics parameters: ";
    cout<<"means: "<<_mean_s<<" "<<_mean_s2<<"  ";
    cout<<"sigmas: "<<sqrt(_sigma_s)<<" "<<sqrt(_sigma_s2)<<"  ";
    cout<<"proportions: "<<_mix_s<<" "<<1-_mix_s<<endl;
    cout<<"Slice weights: ";
    for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
      cout<<_slice_weight[inputIndex]<<" ";
    cout<<endl;
    cout.flush();
  }

}

void irtkReconstruction::Scale()
{
  uint inputIndex;
  int i,j,k,n;
  irtkRealImage slice,w,b,sim;
  POINT p;
  
  double eb;
  double scalenum=0, scaleden=0;
  
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {
    // read the current slice
    slice=_slices[inputIndex];

    //read the current weight image
    w=_weights[inputIndex];
    //read the current bias image
    b=_bias[inputIndex];
    
    //initialise calculation of scale
    scalenum=0;
    scaleden=0;
    
    //Calculate simulated slice
    sim=slice;
    ClearImage(sim,0);

    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
  	  n=_volcoeffs[inputIndex][i][j].size();
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    sim(i,j,0) += p.value*_reconstructed(p.x,p.y,p.z);  
	  }
	  
	  //scale - intensity matching
	  eb=exp(-b(i,j,0));
	  scalenum += w(i,j,0)*slice(i,j,0)*eb*sim(i,j,0);
	  scaleden += w(i,j,0)*slice(i,j,0)*eb*slice(i,j,0)*eb;  
	}
    

    //calculate scale for this slice
    if (scaleden>0)
      _scale[inputIndex]=scalenum/scaleden;
    else _scale[inputIndex] = 1;

   //end of loop for a slice inputIndex  
  }
  
  //Normalise scales by setting geometric mean to 1
  double product=1;
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
    product*=_scale[inputIndex];
  product=pow(product,1.0/_slices.size());
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
    _scale[inputIndex]/=product; 

  if (_debug)
  {
    cout<<setprecision(3);
    cout<<"Slice scale = ";
    for (inputIndex = 0; inputIndex < _slices.size(); ++inputIndex)
      cout<<_scale[inputIndex]<<" ";
    cout<<endl;
    cout.flush();
  }
}


void irtkReconstruction::Bias()
{
  if (_debug)
    cout<<"Correcting bias ...";
  uint inputIndex;
  int i,j,k,n;
  irtkRealImage slice,w,b,sim,wb,deltab,wresidual;
  POINT p;
  double eb,sum,num;
  double scale;
  
  for (inputIndex = 0; inputIndex < _slices.size(); inputIndex++)
  {
    // read the current slice
    slice=_slices[inputIndex];
    //read the current weight image
    w=_weights[inputIndex];
    //read the current bias image
    b=_bias[inputIndex];
    //identify scale factor
    scale = _scale[inputIndex];
    
    
    //prepare weight image for bias field
    wb=w;
    
    
    //simulated slice
    sim=slice;
    ClearImage(sim,0);
    wresidual=sim;
    
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
	  //calculate simulated slice
  	  n=_volcoeffs[inputIndex][i][j].size();
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    sim(i,j,0) += p.value*_reconstructed(p.x,p.y,p.z);  
	  }
	  
	  //bias-correct and scale current slice
	  eb=exp(-b(i,j,0));
	  slice(i,j,0)*=(eb*scale);
	  
	  //calculate weight image
	  wb(i,j,0)=w(i,j,0)*slice(i,j,0);
	  
	  //calculate weighted residual image
	  //make sure it is far from zero to avoid numerical instability
	  if ((sim(i,j,0)>1)&&(slice(i,j,0))>1)
	  {
	    wresidual(i,j,0)=log(slice(i,j,0)/sim(i,j,0))*wb(i,j,0);
	  }
	  else
	  {
	    //do not take into account this voxel when calculating bias field
	    wresidual(i,j,0)=0;
	    wb(i,j,0)=0;
	  }
	}

    //calculate biasfield for this slice    
    
    //smooth weighted residual
    _gb->SetInput(&wresidual);
    _gb->SetOutput(&wresidual);
    _gb->Run();    
     
    //smooth weight image
    _gb->SetInput(&wb);
    _gb->SetOutput(&wb);
    _gb->Run();

    //update biasfield
    sum=0;num=0;
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
	  if (wb(i,j,0)>0)
	    b(i,j,0)+=wresidual(i,j,0)/wb(i,j,0);
	  sum+=b(i,j,0);
	  num++;
	}    

    //normalize bias field to have zero mean
    double mean=0;
    if (num>0)
      mean=sum/num;
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if ((slice(i,j,0)!=-1)&&(num>0))
	{
          b(i,j,0)-=mean;
	}
	
    _bias[inputIndex]=b;

   //end of loop for a slice inputIndex  
  }
  if(_debug)
    cout<<"done. "<<endl;
}


void irtkReconstruction::SuperresolutionAndMStep(int iter)
{
  uint inputIndex;
  int i,j,k,n;
  irtkRealImage slice,addon,w,b,original;
  POINT p;
  double sigma=0, mix=0,num=0,scale;
  double min=0,max=0;
  
  //Remember current reconstruction for edge-preserving smoothing
  original=_reconstructed;
  
  //Clear addon
  addon=_reconstructed;
  ClearImage(addon,0);
  
  //Clear confidence map
  _confidence_map=_reconstructed;
  ClearImage(_confidence_map,0);
   
  for (inputIndex = 0; inputIndex < _slices.size(); ++inputIndex)
  {
    // read the current slice
    slice=_slices[inputIndex];  
    //read the current weight image
    w=_weights[inputIndex];
    //read the current bias image
    b=_bias[inputIndex];
    //identify scale factor
    scale = _scale[inputIndex];

 
    //calculate error
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
	  //bias correct and scale the slice
	  slice(i,j,0)*=exp(-b(i,j,0))*scale;
	  
	  //calculate error
  	  n=_volcoeffs[inputIndex][i][j].size();
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    slice(i,j,0)-=p.value*_reconstructed(p.x,p.y,p.z);
	  }
	  
 	  //sigma and mix
	  double e=slice(i,j,0);
  	  sigma += e*e*w(i,j,0);
	  mix += w(i,j,0);

	  //_m
	  if (e<min) min=e;
	  if (e>max) max=e;
	  
	  num++;
	}	
    
    //Update reconstructed volume using current slice

    //Clear addon
    addon=_reconstructed;
    addon(0,0,0)=1;
    addon.PutMinMax(0,0);

     //Distribute error to the volume
    for (i=0;i<slice.GetX();i++)
      for (j=0;j<slice.GetY();j++)
        if (slice(i,j,0)!=-1)
	{
	  n=_volcoeffs[inputIndex][i][j].size();
	  for(k=0;k<n;k++)
	  {
	    p=_volcoeffs[inputIndex][i][j][k];
	    addon(p.x,p.y,p.z) += p.value*slice(i,j,0)*w(i,j,0)*_slice_weight[inputIndex];
	    _confidence_map(p.x,p.y,p.z) += p.value*w(i,j,0)*_slice_weight[inputIndex];
	  }
	}
	
    //update volume using errors of the current slice
    _reconstructed += addon*_alpha;

   //end of loop for a slice inputIndex  
  }
  
  //bound the intensities
  for(i=0;i<_reconstructed.GetX();i++)
    for(j=0;j<_reconstructed.GetY();j++)
      for(k=0;k<_reconstructed.GetZ();k++)
      {
	  if (_reconstructed(i,j,k)<_min_intensity*0.9) _reconstructed(i,j,k)=_min_intensity*0.9;
	  if (_reconstructed(i,j,k)>_max_intensity*1.1) _reconstructed(i,j,k)=_max_intensity*1.1;
      }
      
  //Calculate sigma and mix
  if (mix>0)
  {
    _sigma=sigma/mix;
  }
  else
  {
    cerr<<"Something went wrong: sigma="<<sigma<<" mix="<<mix<<endl;
    exit(1);
  }
  if (_sigma<_step*_step/6.28)
    _sigma = _step*_step/6.28;
  if (iter>1)
    _mix = mix/num;
  
  //Calculate m
  _m=1/(max-min);

  if (_debug)
  {
    cout<<"Voxel-wise robust statistics parameters: ";
    cout<<"sigma = "<<sqrt(_sigma)<<" mix = "<<_mix<<" ";
    cout<<" m = "<<_m<<endl;
  }
  
  //Smooth the reconstructed image
  AdaptiveRegularization(iter, original);
}



void irtkReconstruction::AdaptiveRegularization(int iter, irtkRealImage& original)
{
  int i,j;
  int directions[13][3]=
  {
    {1,0,-1},{0,1,-1},{1,1,-1},{1,-1,-1},
    {1,0, 0},{0,1, 0},{1,1, 0},{1,-1, 0},
    {1,0, 1},{0,1, 1},{1,1, 1},{1,-1, 1},
    {0,0, 1}
  };
  
  double factor[13]={0,0,0,0,0,0,0,0,0,0,0,0,0};
  for (i=0;i<13;i++)
  {
    for (j=0;j<3;j++)
      factor[i]+= fabs(directions[i][j]);
    factor[i]=1/factor[i];
  }
  
  int dx,dy,dz,x,y,z,xx,yy,zz;
  irtkRealImage b[13];
  double diff,val,sum,valW;
  
  dx=_reconstructed.GetX();
  dy=_reconstructed.GetY();
  dz=_reconstructed.GetZ();
  
  for (i=0;i<13;i++)
    b[i]=_reconstructed;
  for (i=0;i<13;i++)
    for(x=0;x<dx;x++)
      for(y=0;y<dy;y++)
        for(z=0;z<dz;z++)
	{
	  xx=x+directions[i][0];
	  yy=y+directions[i][1];
	  zz=z+directions[i][2];
	  if( (xx>=0)&&(xx<dx)&&(yy>=0)&&(yy<dy)&&(zz>=0)&&(zz<dz)&&(_confidence_map(x,y,z)>0)&&(_confidence_map(xx,yy,zz)>0) )
	  {
	    diff=(original(xx,yy,zz)-original(x,y,z))*sqrt(factor[i])/_delta;
	    b[i](x,y,z)=factor[i]/sqrt(1+diff*diff);

	  }
	  else
	    b[i](x,y,z)=0;
	} 
  
  for(x=0;x<dx;x++)
    for(y=0;y<dy;y++)
      for(z=0;z<dz;z++)
      {
	val=0;
	valW=0;
	sum=0;
        for (i=0;i<13;i++)
	{
  	  xx=x+directions[i][0];
	  yy=y+directions[i][1];
	  zz=z+directions[i][2];
	  if( (xx>=0)&&(xx<dx)&&(yy>=0)&&(yy<dy)&&(zz>=0)&&(zz<dz))
	  {
	    val+=b[i](x,y,z)*_reconstructed(xx,yy,zz)*_confidence_map(xx,yy,zz);
	    valW+=b[i](x,y,z)*_confidence_map(xx,yy,zz);
	    sum+=b[i](x,y,z);
	  }
	}

        for (i=0;i<13;i++)
	{
  	  xx=x-directions[i][0];
	  yy=y-directions[i][1];
	  zz=z-directions[i][2];
	  if( (xx>=0)&&(xx<dx)&&(yy>=0)&&(yy<dy)&&(zz>=0)&&(zz<dz))
	  {
	    val+=b[i](xx,yy,zz)*_reconstructed(xx,yy,zz)*_confidence_map(xx,yy,zz);;
	    valW+=b[i](xx,yy,zz)*_confidence_map(xx,yy,zz);;
	    sum+=b[i](xx,yy,zz);
	  }
	}

        val-=sum*_reconstructed(x,y,z)*_confidence_map(x,y,z);
	valW-=sum*_confidence_map(x,y,z);
	val=_reconstructed(x,y,z)*_confidence_map(x,y,z)+_alpha*_lambda/(_delta*_delta)*val;
	valW=_confidence_map(x,y,z)+_alpha*_lambda/(_delta*_delta)*valW;

	if(valW>0)
	{
	  _reconstructed(x,y,z)=val/valW;
	}
	else _reconstructed(x,y,z)=0;
      }
      
  if (_alpha*_lambda/(_delta*_delta) >0.068)
  {
    cerr<<"Warning: regularization might not have smoothing effect! Ensure that alpha*lambda/delta^2 is below 0.068."<<endl;
  }
}

void irtkReconstruction::MaskVolume()
{
  irtkRealPixel *pr = _reconstructed.GetPointerToVoxels();
  irtkRealPixel *pm = _mask.GetPointerToVoxels();
  for (int i=0; i<_reconstructed.GetNumberOfVoxels();i++)
  {
    if (*pm==0)
      *pr=-1;
    pm++;  
    pr++;
  }
}

void irtkReconstruction::Evaluate(int iter)
{
  cout<<"Iteration "<<iter<<": "<<endl;
  
  cout<<"Included slices: ";
  int sum=0;
  uint i;
  for (i=0; i<_slices.size(); i++)
  {
    if ((_slice_weight[i]>=0.5)&&(_slice_inside[i]))
    {
      cout<<i<<" ";
      sum++;
    }
  }
  cout<<endl<<"Total: "<<sum<<endl;
  
  cout<<"Excluded slices: ";
  sum=0;
  for (i=0; i<_slices.size(); i++)
  {
    if ((_slice_weight[i]<0.5)&&(_slice_inside[i]))
    {
      cout<<i<<" ";
      sum++;
    }
  }
  cout<<endl<<"Total: "<<sum<<endl;
  
  cout<<"Outside slices: ";
  sum=0;
  for (i=0; i<_slices.size(); i++)
  {
    if (!(_slice_inside[i]))
    {
      cout<<i<<" ";
      sum++;
    }
  }
  cout<<endl<<"Total: "<<sum<<endl;

}


