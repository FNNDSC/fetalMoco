#include <irtkImage.h>
#include <irtkTransformation.h>
#include <irtkReconstruction.h>
#include <vector>
using namespace std;

void usage()
{
  cerr << "Usage: reconstruction [reconstructed] [N] [stack_1] .. [stack_N] [dof_1] .. [dof_N] <options>\n" << endl;
  cerr << endl;

  cerr << "\t[reconstructed]         Name for the reconstructed volume. Nifti or Analyze format." << endl;
  cerr << "\t[N]                     Number of stacks." << endl;
  cerr << "\t[stack_1] .. [stack_N]  The input stacks. Nifti or Analyze format." << endl;
  cerr << "\t[dof_1]   .. [dof_N]    The transformations of the input stack to template" << endl;
  cerr << "\t                        in \'dof\' format used in IRTK." <<endl;
  cerr << "\t                        Only rough alignment with correct orienation and " << endl;
  cerr << "\t                        some overlap is needed." << endl;
  cerr << "\t                        Use \'id\' for an identity transformation for at least" << endl;
  cerr << "\t                        one stack. The first stack with \'id\' transformation" << endl;
  cerr << "\t                        will be resampled as template." << endl;
  cerr << "\t" << endl;
  cerr << "Options:" << endl;
  cerr << "\t-thickness [th_1] .. [th_N] Give slice thickness.[Default: voxel size in z direction]"<<endl;
  cerr << "\t-mask [mask]            Binary mask to define the region od interest. [Default: whole image]"<<endl;
  cerr << "\t-iterations [iter]      Number of registration-reconstruction iterations. [Default: 9]"<<endl;
  cerr << "\t-sigma [sigma]          Stdev for bias field. [Default: 12mm]"<<endl;
  cerr << "\t-resolution [res]       Isotropic resolution of the volume. [Default: 0.75mm]"<<endl;
  cerr << "\t-multires [levels]      Multiresolution smooting with given number of levels. [Default: 3]"<<endl;
  cerr << "\t-average [average]      Average intensity value for stacks [Default: 700]"<<endl;
  cerr << "\t-delta [delta]          Parameter to define what is an edge. [Default: 150]"<<endl;
  cerr << "\t-lambda [lambda]        Smoothing parameter. [Default: 0.02]"<<endl;
  cerr << "\t-lastIter [lambda]      Smoothing parameter for last iteration. [Default: 0.01]"<<endl;
  cerr << "\t-smooth_mask [sigma]    Smooth the mask to reduce artefacts of manual segmentation. [Default: 4mm]"<<endl;
  cerr << "\t-debug                  Debug mode - save intermediate results."<<endl;
  cerr << "\t" << endl;
  cerr << "\t" << endl;
  exit(1);
}

int main(int argc, char **argv)
{
  
  //utility variables
  int i, ok;
  char buffer[256];
  irtkRealImage stack; 

  //declare variables for input
  /// Name for output volume
  char * output_name = NULL;
  /// Slice stacks
  vector<irtkRealImage> stacks;
  /// Stack transformation
  vector<irtkRigidTransformation> stack_transformations;
  /// Stack thickness
  vector<double > thickness;
  ///number of stacks
  int nStacks;
    
  // Default values.
  int templateNumber=-1;
  irtkRealImage *mask=NULL;
  int iterations = 9;
  bool debug = false;
  double sigma=12;
  double resolution = 0.75;
  double lambda = 0.02;
  double delta = 150;
  int levels = 3;
  double lastIterLambda = 0.01;
  int rec_iterations;
  double averageValue = 700;
  double smooth_mask = 4;
  
  //if not enough arguments print help
  if (argc < 5)
    usage();
  
  //read output name
  output_name = argv[1];
  argc--;
  argv++;
  cout<<"Recontructed volume name ... "<<output_name<<endl;
  cout.flush();

  //read number of stacks
  nStacks = atoi(argv[1]);
  argc--;
  argv++;
  cout<<"Number 0f stacks ... "<<nStacks<<endl;
  cout.flush();

  // Read stacks 
  for (i=0;i<nStacks;i++)
  {
    stack.Read(argv[1]);
    cout<<"Reading stack ... "<<argv[1]<<endl;
    cout.flush();
    argc--;
    argv++;
    stacks.push_back(stack);
  }
  
  //Read transformation
  for (i=0;i<nStacks;i++)
  {
    irtkTransformation *transformation;
    cout<<"Reading transformation ... "<<argv[1]<<" ... ";
    cout.flush();
    if (strcmp(argv[1], "id") == 0)
    {
      transformation = new irtkRigidTransformation;
      if ( templateNumber < 0) templateNumber = i;
    }
    else
    {
      transformation = irtkTransformation::New(argv[1]);
    }
    cout<<" done."<<endl;
    cout.flush();

    argc--;
    argv++;
    irtkRigidTransformation *rigidTransf = dynamic_cast<irtkRigidTransformation*> (transformation);
    stack_transformations.push_back(*rigidTransf);
    delete rigidTransf;
  }

  // Parse options.
  while (argc > 1){
    ok = false;
    
    //Read slice thickness
    if ((ok == false) && (strcmp(argv[1], "-thickness") == 0)){
      argc--;
      argv++;
      cout<< "Slice thickness is ";
      for (i=0;i<nStacks;i++)
      {
        thickness.push_back(atof(argv[1]));
	cout<<thickness[i]<<" ";
        argc--;
        argv++;
       }
       cout<<"."<<endl;
       cout.flush();
      ok = true;
    }

    //Read binary mask for final volume
    if ((ok == false) && (strcmp(argv[1], "-mask") == 0)){
      argc--;
      argv++;
      mask= new irtkRealImage(argv[1]);
      ok = true;
      argc--;
      argv++;
    }
    
    //Read number of registration-reconstruction iterations
    if ((ok == false) && (strcmp(argv[1], "-iterations") == 0)){
      argc--;
      argv++;
      iterations=atoi(argv[1]);
      ok = true;
      argc--;
      argv++;
    }

    //Variance of Gaussian kernel to smooth the bias field.
    if ((ok == false) && (strcmp(argv[1], "-sigma") == 0)){
      argc--;
      argv++;
      sigma=atof(argv[1]);
      ok = true;
      argc--;
      argv++;
    }
    
    //Smoothing parameter
    if ((ok == false) && (strcmp(argv[1], "-lambda") == 0)){
      argc--;
      argv++;
      lambda=atof(argv[1]);
      ok = true;
      argc--;
      argv++;
    }
    
    //Smoothing parameter for last iteration
    if ((ok == false) && (strcmp(argv[1], "-lastIter") == 0)){
      argc--;
      argv++;
      lastIterLambda=atof(argv[1]);
      ok = true;
      argc--;
      argv++;
    }
    
    //Parameter to define what is an edge
    if ((ok == false) && (strcmp(argv[1], "-delta") == 0)){
      argc--;
      argv++;
      delta=atof(argv[1]);
      ok = true;
      argc--;
      argv++;
    }
    
    //Isotropic resolution for the reconstructed volume
    if ((ok == false) && (strcmp(argv[1], "-resolution") == 0)){
      argc--;
      argv++;
      resolution=atof(argv[1]);
      ok = true;
      argc--;
      argv++;
    }

    //Number of resolution levels
    if ((ok == false) && (strcmp(argv[1], "-multires") == 0)){
      argc--;
      argv++;
      levels=atoi(argv[1]);
      argc--;
      argv++;
      ok = true;
    }

    //Smooth mask to remove effects of manual segmentation
    if ((ok == false) && (strcmp(argv[1], "-smooth_mask") == 0)){
      argc--;
      argv++;
      smooth_mask=atof(argv[1]);
      argc--;
      argv++;
      ok = true;
    }

    //Debug mode
    if ((ok == false) && (strcmp(argv[1], "-debug") == 0)){
      argc--;
      argv++;
      debug=true;
      ok = true;
    }

    if (ok == false){
      cerr << "Can not parse argument " << argv[1] << endl;
      usage();
    }
  }
  
  //Initialise slice thickness if not given by user
  if (thickness.size()==0)
  {
    cout<< "Slice thickness is ";
    for (i=0;i<nStacks;i++)
    {
      double dx,dy,dz;
      stacks[i].GetPixelSize(&dx,&dy,&dz);
      thickness.push_back(dz);
      cout<<thickness[i]<<" ";
    }
    cout<<"."<<endl;
    cout.flush();
  }

  //Create reconstruction object
  irtkReconstruction reconstruction;
  
  //Output volume
  irtkRealImage reconstructed;

  //Set debug mode
  if (debug) reconstruction.DebugOn();
  else reconstruction.DebugOff();

  
  // Check whether the template stack can be indentified
  if (templateNumber<0)
  {
    cerr<<"Please identify the template by assigning id transformation."<<endl;
    exit(1);
  }  
  
  //Before creating the template we will crop template stack according to the given mask
  if (mask !=NULL)
  {
    //first resample the mask to the space of the stack
    //for template stact the transformation is identity
    irtkRealImage m = *mask;
    reconstruction.TransformMask(stacks[templateNumber],m,stack_transformations[templateNumber]);
    //Crop template stack
    reconstruction.CropImage(stacks[templateNumber],m);
    if (debug)
    {
      m.Write("maskTemplate.nii.gz"); 
      stacks[templateNumber].Write("croppedTemplate.nii.gz");
    }
  }
  
  //Create template volume with isotropic resolution 
  //if resolution==0 it will be determined from in-plane resolution of the image
  resolution = reconstruction.CreateTemplate(stacks[templateNumber],resolution);
  
  //Set mask to reconstruction object. 
  reconstruction.SetMask(mask,smooth_mask);   

  //to redirect output from screen to text files
  
  //to remember cout and cerr buffer
  streambuf* strm_buffer = cout.rdbuf();
  streambuf* strm_buffer_e = cerr.rdbuf();
  //files for registration output
  ofstream file("log-registration.txt");
  ofstream file_e("log-registration-error.txt");
  //files for reconstruction output
  ofstream file2("log-reconstruction.txt");
  ofstream fileEv("log-evaluation.txt");
  
  //set precision
  cout<<setprecision(3);
  cerr<<setprecision(3);

  //perform volumetric registration of the stacks
  //redirect output to files
  cerr.rdbuf(file_e.rdbuf());
  cout.rdbuf (file.rdbuf());
  //volumetric registration
  reconstruction.StackRegistrations(stacks,stack_transformations,templateNumber);
  cout<<endl;
  cout.flush();
  //redirect output back to screen
  cout.rdbuf (strm_buffer);
  cerr.rdbuf (strm_buffer_e);
  
  //Volumetric registrations are stack-to-template while slice-to-volume 
  //registrations are actually performed as volume-to-slice (reasons: technicalities of implementation)
  //Need to invert stack transformations now.  
  reconstruction.InvertStackTransformations(stack_transformations);
  
  //Mask is transformed to the all other stacks and they are cropped
  for (i=0; i<nStacks; i++)
  {
    //template stack has been cropped already
    if (i==templateNumber) continue;
    //transform the mask
    irtkRealImage m=reconstruction.GetMask();
    reconstruction.TransformMask(stacks[i],m,stack_transformations[templateNumber]);
    //Crop template stack
    reconstruction.CropImage(stacks[i],m);
    if (debug)
    {
      sprintf(buffer,"mask%i.nii.gz",i);
      m.Write(buffer); 
      sprintf(buffer,"cropped%i.nii.gz",i);
      stacks[i].Write(buffer);
    }
  }
  
  //Repeat volumetric registrations with cropped stacks
  reconstruction.InvertStackTransformations(stack_transformations);
  //redirect output to files
  cerr.rdbuf(file_e.rdbuf());
  cout.rdbuf (file.rdbuf());
  //volumetric registration
  reconstruction.StackRegistrations(stacks,stack_transformations,templateNumber);
  cout<<endl;
  cout.flush();
  //redirect output back to screen
  cout.rdbuf (strm_buffer);
  cerr.rdbuf (strm_buffer_e);
  reconstruction.InvertStackTransformations(stack_transformations);
  
  //Rescale intensities of the stacks to have the same average
  reconstruction.MatchStackIntensities(stacks,stack_transformations,averageValue);

  //Create slices and slice-dependent transformations
  reconstruction.CreateSlicesAndTransformations(stacks,stack_transformations,thickness);
  
  //Mask all the slices
  reconstruction.MaskSlices();
   
  //Set sigma for the bias field smoothing
  if (sigma>0)
    reconstruction.SetSigma(sigma);
  else
  {
    cerr<<"Please set sigma larger than zero. Current value: "<<sigma<<endl;
    exit(1);
  }
    
  //Initialise data structures for EM
  reconstruction.InitializeEM();
  
  
  //interleaved registration-reconstruction iterations
  for (int iter=0;iter<iterations;iter++)
  {
    //Print iteration number on the screen
    cout.rdbuf (strm_buffer);
    cout<<"Iteration "<<iter<<". "<<endl;
    cout.flush();

    //perform slice-to-volume registrations - skip the first iteration 
    if (iter>0)
    {
      cerr.rdbuf(file_e.rdbuf());
      cout.rdbuf (file.rdbuf());
      cout<<"Iteration "<<iter<<": "<<endl;
      reconstruction.SliceToVolumeRegistration();
      cout<<endl;
      cout.flush();
      cerr.rdbuf (strm_buffer_e);
    }

    
    //Write to file
    cout.rdbuf (file2.rdbuf());
    cout<<endl<<endl<<"Iteration "<<iter<<": "<<endl<<endl;
    
    //Set smoothing parameters 
    //amount of smoothing (given by lambda) is decreased with improving alignment
    //delta (to determine edges) stays constant throughout
    if(iter==(iterations-1))
      reconstruction.SetSmoothingParameters(delta,lastIterLambda);
    else
    {
      double l=lambda;
      for (i=0;i<levels;i++)
      {
        if (iter==iterations*(levels-i-1)/levels)
          reconstruction.SetSmoothingParameters(delta, l);
        l*=2;
      }
    }
    
    //Use faster reconstruction during iterations and slower for final reconstruction
    if ( iter<(iterations-1) )
      reconstruction.SpeedupOn();
    else 
      reconstruction.SpeedupOff();
    
    //Initialise values of weights, scales and bias fields
    reconstruction.InitializeEMValues();
    
    //Calculate matrix of transformation between voxels of slices and volume
    reconstruction.CoeffInit();
    
    //Initialize reconstructed image with Gaussian weighted reconstruction
    reconstruction.GaussianReconstruction();
    
    //Initialize robust statistics parameters
    reconstruction.InitializeRobustStatistics();
    
    //EStep
    reconstruction.EStep();

    //number of reconstruction iterations
    if ( iter==(iterations-1) ) 
    {
      rec_iterations = 30;      
    }
    else 
      rec_iterations = 10;
        
    //reconstruction iterations
    i=0;
    for (i=0;i<rec_iterations;i++)
    {
      cout<<endl<<"  Reconstruction iteration "<<i<<". "<<endl;
      
      //calculate scales
      reconstruction.Scale();
      
      //calculate bias fields
      reconstruction.Bias();
      
      //MStep and update reconstructed volume
      reconstruction.SuperresolutionAndMStep(i+1);
      
      //E-step
      reconstruction.EStep();
      
    }//end of reconstruction iterations
    
    //Mask reconstructed image to ROI given by the mask
    reconstruction.MaskVolume();

    //Save reconstructed image
    if (debug)
    {
      reconstructed=reconstruction.GetReconstructed();
      sprintf(buffer,"image%i.nii.gz",iter);
      reconstructed.Write(buffer);
    }

   //Evaluate - write number of included/excluded/outside/zero slices in each iteration in the file
   cout.rdbuf (fileEv.rdbuf());
   reconstruction.Evaluate(iter);
   cout<<endl;
   cout.flush();
   cout.rdbuf (strm_buffer); 
   
  }// end of interleaved registration-reconstruction iterations

  //save final result
  reconstructed=reconstruction.GetReconstructed();
  reconstructed.Write(output_name); 
  reconstruction.SaveTransformations();
  reconstruction.SaveSlices();
  
  //The end of main()
}  
