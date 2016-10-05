/*
---------------------------------------------------------------------------------------
CS599 Project 2
R Mitchell Ralston (rmr5)
9/20/16
--------------------------
Implementation of Project2 as per assignment spec on BBLearn

Objectives of the for raycast.c
----------------------------------------

Organization:
------------

Workflow:
--------

Issues:
-------
TODO: make the background color black, this is the standard default for ray tracers (maybe I can make a white background)

Questions:
---------
- should we support planes that have a [1,0,0] normal? -> degenerate cases, have an infinite number of
  intersections, he doesn't care if we color or not
- 1 or 2 sided planes? (what to do when plane is facing away from the ray) -> color both sides of the plane the same

look at iphone pic from 10/4 and you can see that D is the dot product of the origin with the plane normal

---------------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define DEFAULT_COLOR 0

// typdefs
typedef struct RGBPixel {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} RGBPixel ;

typedef struct RGBAPixel {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
} RGBAPixel ;

typedef struct PPM_file_struct {
  char magic_number;
  int width;
  int height;
  int alpha;
  int depth;
  char *tupltype;
  FILE* fh_out;
} PPM_file_struct ;

typedef struct x_y_z {
  double x;
  double y;
  double z;
} x_y_z ;

typedef struct A_J {
  double A;
  double B;
  double C;
  double D;
  double E;
  double F;
  double G;
  double H;
  double I;
  double J;
} A_J ;

typedef struct JSON_object {
  char *type; // helpful to have both string and number reference for this
  int  typecode; // 0 = camera, 1 = sphere, 2 = plane, 3 = cylinder, 4 = quadric
  double width;
  double height;
  x_y_z color;
  x_y_z position;
  x_y_z normal;
  double center[3];
  double radius;
  A_J coeffs;    // quadric coefficients
} JSON_object ;

// This may not be the best approach, and it's certainly not most efficient - to have an array that
// is always 128 "JSON_object"s large. But it's clean and all of the data related to the JSON
// scene file is in this one struct, filehandle and all, that's what I like about it.
typedef struct JSON_file_struct {
  FILE* fh_in;
  int width;
  int height;
  int num_objects;
  JSON_object js_objects[128];
} JSON_file_struct ;

typedef double* V3;

// inline functions:
static inline double sqr (double v) {
  return v * v;
}

static inline void vNormalize (double* v) {
  double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}

static inline void vAdd(V3 a, V3 b, V3 c) {
  c[0] = a[0] + b[0];
  c[1] = a[1] + b[1];
  c[2] = a[2] + b[2];
}

static inline void vSubtract(V3 a, V3 b, V3 c) {
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  c[2] = a[2] - b[2];
}

static inline double vDot(V3 a, V3 b) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static inline double vNorm(V3 a) {
  return sqrt(vDot(a,a));
}

static inline double vDist(V3 a, V3 b) {
  V3 diff;
  vSubtract(a, b, diff);
  return vNorm(diff);
  //  return vNormalize(a-b);
}

static inline void vScale(V3 a, double s, V3 c) {
  c[0] = s * a[0];
  c[1] = s * a[1];
  c[2] = s * a[2];
}

// helper to convert an x_y_z struct into an array of doubles
static inline void convertXYZ (x_y_z xyz, double* array) {
  array[0] = xyz.x;
  array[1] = xyz.y;
  array[2] = xyz.z;
}

// dist_Point_to_Plane(): get distance (and perp base) from a point to a plane
//    Input:  P  = a 3D point
//            PL = a  plane with point V0 and normal n
//    Output: *B = base point on PL of perpendicular from P
//    Return: the distance from P to the plane PL
 double vDistToPlane( double* point, x_y_z plane_point, x_y_z plane_normal) {
  double numer;
  double denom;
  double sb;

  double* tmp;
  double t_pp[3];
  convertXYZ(plane_point,t_pp);
  vSubtract(point,t_pp,tmp);
  double* base;

  double t_pn[3];
  convertXYZ(plane_normal,t_pn);

  numer = -vDot( t_pn, tmp);
  denom = vDot(t_pn, t_pn);
  sb = numer / denom;

  double t_ts[3];
  vScale(t_pn,sb,t_ts);
  double t_ta[3];
  vAdd(point,t_ts,t_ta);
  return vDist(point, t_ta);
}  

// END inline functions

// global variables
int CURRENT_CHAR        = 'a';
int OUTPUT_MAGIC_NUMBER = 6; // default to P6 PPM format
int VERBOSE             = 0; // controls logfile message level
int ASCII_IMAGE         = 0; // controls if there is an ascii image printed to terminal while raycasting
int INFO                = 1; // controls if Info messages are printed, turn off prior to submission

// global data structures
JSON_file_struct    INPUT_FILE_DATA;
RGBPixel           *RGB_PIXEL_MAP;
PPM_file_struct     OUTPUT_FILE_DATA;
RGBAPixel          *RGBA_PIXEL_MAP;

// functions
void  writePPM        (char *outfile,         PPM_file_struct *input);
void  message         (char message_code[],   char message[]        );
void  writePPMHeader  (FILE* fh              );
void  reportPPMStruct (PPM_file_struct *input);
void  reportPixelMap  (RGBPixel *pm          );
void  checkJSON       (JSON_object *scene);
void  printJSONObjectStruct (JSON_object jostruct);
void  storeDouble(int obj_count, char* key, double value);
void  storeVector(int obj_count, char* key, double* value);
void  rayCast(JSON_object *scene, RGBPixel *image);
double sphereIntersection  (double* Ro, double* Rd, x_y_z C, double r);
double planeIntersection   (double* Ro, double* Rd, x_y_z C, x_y_z N);
double quadricIntersection (double* Ro, double* Rd, x_y_z C, A_J c);
double cylinderIntersection(double* Ro, double* Rd, x_y_z C, double r);

void   convertXYZ (x_y_z xyz, double* array);
unsigned char shadePixel (double value);

void  help            ();
int   computeDepth();
char* computeTuplType();
void  freeGlobalMemory ();
void  closeAndExit ();
void  reportScene();
double getCameraWidth();
double getCameraHeight();


/* 
 ------------------------------------------------------------------
 Parser and functions from Dr. P - refactored and enhanced
 ------------------------------------------------------------------
*/
int line = 1;

// nextC() wraps the getc() function and provides error checking and line
// number maintenance
int nextC(FILE* json) {
  int c = fgetc(json);
  if (VERBOSE) printf("'%c'", c);
  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    //TODO - message this
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}

// expectC() checks that the next character is d.  If it is not it emits
void expectC(FILE* json, int d) {
  int c = nextC(json);
  if (c == d) return;
  //TODO - message this
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);    
}


// skipWSpace() skips white space in the file.
void skipWSpace(FILE* json) {
  int c = nextC(json);
  while (isspace(c)) {
    c = nextC(json);
  }
  ungetc(c, json);
}


// nextString() gets the next string from the file handle and emits an error
// if a string can not be obtained.
char* nextString(FILE* json) {
  char buffer[129];
  int c = nextC(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }  
  c = nextC(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      exit(1);      
    }
    if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      exit(1);      
    }
    if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = nextC(json);
  }
  buffer[i] = 0;
  if (VERBOSE) printf("\nnS:  returning string--> (%s)\n",buffer);
  return strdup(buffer);
}

double nextNumber(FILE* json) {
  double value;
  fscanf(json, "%lf", &value);
  // TODO: Error checking
  if (VERBOSE) printf("\nnN:  returning number--> (%lf)\n",value);
  return value;
}

double* nextVector(FILE* json) {
  double* v = malloc(3*sizeof(double));
  expectC(json, '[');
  skipWSpace(json);
  v[0] = nextNumber(json);
  skipWSpace(json);
  expectC(json, ',');
  skipWSpace(json);
  v[1] = nextNumber(json);
  skipWSpace(json);
  expectC(json, ',');
  skipWSpace(json);
  v[2] = nextNumber(json);
  skipWSpace(json);
  expectC(json, ']');
  if (VERBOSE) printf("nV: returning vector--> %d\n",v);
  return v;
}

// This is the big JSON parser function
void readScene(char* filename) {
  int c;
  int obj_count = 0;
  
  FILE* json = fopen(filename, "r");

  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    exit(1);
  }
  
  skipWSpace(json);
  
  // Find the beginning of the list
  expectC(json, '[');

  skipWSpace(json);

  // Find all the objects in the JSON scene file
  int fail_safe = 0;
  while (1) {
    /* Error checking */
    // max supported objects * number of JSON lines per object (with margin of error)
    fail_safe++;
    if (fail_safe > 1280) message("Error","Do you have a ']' to terminate your JSON file?");
    if (obj_count > 128) message("Error","Maximum supported number of JSON objects is 128");

    /* Process file */
    c = fgetc(json);
    if (c == ']') {
      message("Error","No objects detected!\n");
      fclose(json);
      return;
    }
    if (c == '{') {
      skipWSpace(json);
      
      // Parse the object, getting the type first
      char* key = nextString(json);
      if (strcmp(key, "type") != 0) {
	fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
	exit(1);
      }
      
      skipWSpace(json);
      expectC(json, ':');
      skipWSpace(json);
      
      // get the type of the object and store it at the index of the current object
      char* value = nextString(json);
      if (strcmp(value, "camera") == 0) {
	if (INFO) message("Info","Processing camera object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "camera";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 0;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "sphere") == 0) {
	if (INFO) message("Info","Processing sphere object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "sphere";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 1;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "plane") == 0) {
	if (INFO) message("Info","Processing plane object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "plane";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 2;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "cylinder") == 0) {
	if (INFO) message("Info","Processing cylinder object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "cylinder";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 3;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "quadric") == 0) {
	if (INFO) message("Info","Processing quadric object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "quadric";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 4;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else {
	fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
	exit(1);
      }
      skipWSpace(json);
      
      // This while processes the attributes of the object
      while (1) {
	// , }
	c = nextC(json);
	if (c == '}') {
	  // stop parsing this object and increment the object counter
	  obj_count++;
	  break;
	} else if (c == ',') {
	  // read another field
	  skipWSpace(json);
	  char* key = nextString(json);
	  skipWSpace(json);
	  expectC(json, ':');
	  skipWSpace(json);
	  //INPUT_FILE_DATA.js_objects[obj_count].width = nextNumber(json);
	  //
	  if ((strcmp(key, "width") == 0) ||
	      (strcmp(key, "height") == 0) ||
	      (strcmp(key, "radius") == 0) ||
	      (strcmp(key, "A") == 0) ||
	      (strcmp(key, "B") == 0) ||
	      (strcmp(key, "C") == 0) ||
	      (strcmp(key, "D") == 0) ||
	      (strcmp(key, "E") == 0) ||
	      (strcmp(key, "F") == 0) ||
	      (strcmp(key, "G") == 0) ||
	      (strcmp(key, "H") == 0) ||
	      (strcmp(key, "I") == 0) ||
	      (strcmp(key, "J") == 0)
	      ) {
	    double value = nextNumber(json);
	    storeDouble(obj_count,key,value);
	  } else if ((strcmp(key, "color") == 0) ||
		     (strcmp(key, "position") == 0) ||
		     (strcmp(key, "normal") == 0)) {
	    double* value = nextVector(json);
	    storeVector(obj_count,key,value);
	  } else {
	    fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",key, line);
	  }
	  skipWSpace(json);
	} else {
	  fprintf(stderr, "Error: Unexpected value on line %d\n", line);
	  exit(1);
	}
      }
      skipWSpace(json);
      c = nextC(json);
      if (c == ',') {
	skipWSpace(json);
      } else if (c == ']') {
	fclose(json);
	return;
      } else {
	// TODO: message this
	fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
	exit(1);
      }
    }
  }
  if (INFO) message("Info","Read scene file");
}
/* 
 ------------------------------------------------------------------
 End pirated code - Parser and functions from Dr. P
 ------------------------------------------------------------------
*/


/*
 ------------------------------------------------------------------
                                 MAIN
 ------------------------------------------------------------------
*/
int main(int argc, char *argv[]) {
  // check for proper number of input args
  if (argc != 5) {
    help();
    return(1);
  }

  // process input arguments and report what is being processed, store some variables
  int width = atoi(argv[1]);
  int height = atoi(argv[2]);
  char *infile = argv[3];
  char *outfile = argv[4];
  if (strcmp(infile,outfile)  == 0) {printf("Error: input and output file names the same!\n"); return EXIT_FAILURE;}
  
  if (INFO) message("Info","Processing the following arguments:");
  printf("          Input : %s\n",infile);
  printf("          Output: %s\n",outfile);
  printf("          Width : %d\n",width);
  printf("          Height: %d\n",height);

  INPUT_FILE_DATA.width = width;
  INPUT_FILE_DATA.height = height;

  // parse the JSON
  //int parse_success = parseJSON(infile);
  // Read scene - code from class
  readScene(infile);
  
  // error checking
  checkJSON(INPUT_FILE_DATA.js_objects);

  // report results (TODO: if VERBOSE)
  reportScene();

  // initialize the image buffer
  RGB_PIXEL_MAP = malloc(sizeof(RGBPixel)  * INPUT_FILE_DATA.width * INPUT_FILE_DATA.height );
  
  // run the raycasting
  rayCast(INPUT_FILE_DATA.js_objects,RGB_PIXEL_MAP);

  // write the image
  writePPM(outfile,&OUTPUT_FILE_DATA);
  
  // prepare to exit
  freeGlobalMemory();
  return EXIT_SUCCESS;
}
/* 
 ------------------------------------------------------------------
                               END MAIN
 ------------------------------------------------------------------
*/



/*
  ------------------------
  FUNCTION IMPLEMENTATIONS
  ------------------------
*/

/*
  --- message ---
  - 9/10/16
  - rmr5
  ---------------
  print a message to stdout consisting of a message code and a message to a given channel
  current valid channels to write to (stdout, stderr, etc) - will add fh later
  //void message (char channel[], char message_code[], char message[]) {
*/
void message (char message_code[], char message[]) {
  if(message_code == "Error") {
    fprintf(stderr,"%s: %s\n",message_code,message);
    closeAndExit();
    exit(-1);
  } else {
    printf("%s: %s\n",message_code,message);
  }
}

/*
  --- help ---
  - rmr5
  ---------------
  print usage to user when arguments invalid
*/
void help () {
  message("Error","Invalid arguments!");
  message("Usage","raycast width height input.json output.ppm");
}

/*
  --- freeGlobalMemory ---
  - 9/15/16
  - rmr5
  ---------------
  free up any globally malloc memory
*/
// TODO: make this more universal
void freeGlobalMemory () {
  free(RGB_PIXEL_MAP);
  free(RGBA_PIXEL_MAP);
}

/*
  --- closeAndExit ---
  - 9/15/16
  - rmr5
  ---------------
  prepare to gracefully exit the program by freeing memory and closing any open filehandles
  TODO: need to finish
*/
void closeAndExit () {
  freeGlobalMemory();
  //fclose(INPUT_FILE_DATA->fh_in);
  exit(-1);
}


//  small helper to assign proper depth to a P7 file
int computeDepth() {
  if ((strcmp(OUTPUT_FILE_DATA.tupltype,"RGB_ALPHA")) == 0) {
    return 4; 
  } else {
    return 3;
  }
}

// helper to assign preper tupltype in P7
char* computeTuplType() {
  if ((strcmp(OUTPUT_FILE_DATA.tupltype,"RGB_ALPHA")) == 0) {
    if (VERBOSE) printf("cD: returning tupltype RGB_ALPHA because input was %s\n",OUTPUT_FILE_DATA.tupltype);
    return "RGB_ALPHA"; 
  } else {
    if (VERBOSE) printf("cD: returning tupltype RGB because input was %s\n",OUTPUT_FILE_DATA.tupltype);
    return "RGB"; 
  }
}

// helper function to write the header to a file handle
void writePPMHeader (FILE* fh) {
  int magic_number = OUTPUT_MAGIC_NUMBER;

  // These values/header elements are the same regardless format
  printf("Info: Converting to format %d ...\n",magic_number);
  fprintf(fh,"P%d\n",magic_number);
  fprintf(fh,"# PPM file format %d\n",magic_number);
  fprintf(fh,"# written by ppmrw(rmr5)\n");
  // make some variable assignments from input -> output
  OUTPUT_FILE_DATA.magic_number = magic_number;
  OUTPUT_FILE_DATA.width        = INPUT_FILE_DATA.width;
  OUTPUT_FILE_DATA.height       = INPUT_FILE_DATA.height;
  OUTPUT_FILE_DATA.alpha        = 255;
  
  if (magic_number == 3 || magic_number == 6) {
    fprintf(fh,"%d %d\n",       OUTPUT_FILE_DATA.width,OUTPUT_FILE_DATA.height);
    fprintf(fh,"%d\n",          OUTPUT_FILE_DATA.alpha);
  } else if (magic_number == 7) {
    OUTPUT_FILE_DATA.depth      = computeDepth();
    OUTPUT_FILE_DATA.tupltype   = computeTuplType();
    
    fprintf(fh,"WIDTH %d\n",    OUTPUT_FILE_DATA.width);
    fprintf(fh,"HEIGHT %d\n",   OUTPUT_FILE_DATA.height);
    fprintf(fh,"DEPTH %d\n",    OUTPUT_FILE_DATA.depth);
    fprintf(fh,"MAXVAL %d\n",   OUTPUT_FILE_DATA.alpha);
    fprintf(fh,"TUPLTYPE %d\n", OUTPUT_FILE_DATA.tupltype);
    fprintf(fh,"ENDHDR\n");
  } else {
    message("Error","Trying to output unsupported format!\n");
  }
  if (INFO) message("Info","Done writing header");
}

/*
  --- writePPM ---
  - 9/13/16
  - rmr5
  ---------------
  Major function to write the actual output ppm file
  takes a output filename and an input PPM struct
  uses global data

  This function has case statements to support all supported formats 
*/
void writePPM (char *outfile, PPM_file_struct *input) {
  printf("Info: Writing file %s...\n",outfile);
  FILE* fh_out = fopen(outfile,"wb");

  // -------------------------- write header ---------------------------------
  writePPMHeader(fh_out);
  // ---------------------- done write header --------------------------------

  // -------------------------- write image ----------------------------------
  int pixel_index = 0;
  int modulo;
  switch(OUTPUT_FILE_DATA.magic_number) {
    // P3 format
    // Iterate over each pixel in the pixel map and write them byte by byte
  case(3):
    if (INFO) message("Info","Outputting format 3");
    while(pixel_index < (OUTPUT_FILE_DATA.width) * (OUTPUT_FILE_DATA.height)) {      
      fprintf(fh_out,"%3d %3d %3d",RGB_PIXEL_MAP[pixel_index].r,RGB_PIXEL_MAP[pixel_index].g,RGB_PIXEL_MAP[pixel_index].b);
      modulo = (pixel_index + 1) % (OUTPUT_FILE_DATA.width);
      if ( modulo == 0 ) {
	fprintf(fh_out,"\n");
      } else {
	fprintf(fh_out," ");
      }
      pixel_index++;
    }
    break;
    // P6 format
    // write the entire pixel_map in one command
  case(6):
    if (INFO) message("Info","Outputting format 6");
    fwrite(RGB_PIXEL_MAP, sizeof(RGBPixel), OUTPUT_FILE_DATA.width * OUTPUT_FILE_DATA.height, fh_out);
    break;
    // P7 format
  case(7):
    // write the entire pixel_map in one command, RGB writes from RGB pixel_map and RGBA writes from RGBA pixel_map
    if (INFO) message("Info","Outputting format 7");
    if (strcmp(OUTPUT_FILE_DATA.tupltype,"RGB_ALPHA") == 0) {
      if (INFO) message("Info","   output file will have alpha data");
      fwrite(RGBA_PIXEL_MAP, sizeof(RGBAPixel), OUTPUT_FILE_DATA.width * OUTPUT_FILE_DATA.height, fh_out);
    } else {
      if (INFO) message("Info","   output file is RGB only");
      fwrite(RGB_PIXEL_MAP, sizeof(RGBPixel), OUTPUT_FILE_DATA.width * OUTPUT_FILE_DATA.height, fh_out);
    }
    break;
  default:
    message("Error","Unrecognized output format");
  }
  // ---------------------- done write image ---------------------------------

  fclose(fh_out);
  reportPPMStruct(&OUTPUT_FILE_DATA);
  if (INFO) message("Info","Done ");
}

// helper function to visualize what's in a given PPM struct
void reportPPMStruct (PPM_file_struct *input) {
  if (INFO) message("Info","Contents of PPM struct:");
  printf("     magic_number: %d\n",input->magic_number);
  printf("     width:        %d\n",input->width);
  printf("     height:       %d\n",input->height);
  if (input->magic_number == 7) {
  printf("     max_value:    %d\n",input->alpha);
  printf("     depth:        %d\n",input->depth);
  printf("     tupltype:     %s\n",input->tupltype);
  } else {
    printf("     alpha:        %d\n",input->alpha);
  }
}

// small utility function to print the contents of a pixelMap
void reportPixelMap (RGBPixel *pm) {
  int index = 0;
  int fail_safe = 0;
  while(index < sizeof(pm) && fail_safe < 1000) {
    printf("rPM: [%d] = [%d,%d,%d]\n",index,pm[index].r,pm[index].g,pm[index].b);
    index++;
    fail_safe++;
  }
}

// helper to print out a JSON_object
void printJSONObjectStruct (JSON_object jostruct) {
  printf("type: %s\n",jostruct.type);
  if (strcmp(jostruct.type,"camera") == 0) {
    printf(" width: %f\n",jostruct.width);
    printf("height: %f\n",jostruct.height);
  } else if (strcmp(jostruct.type,"sphere") == 0) {
    printf("    color: [%f, %f, %f]\n",jostruct.color.x, jostruct.color.y, jostruct.color.z);
    printf(" position: [%f, %f, %f]\n",jostruct.position.x, jostruct.position.y, jostruct.position.z);
    printf("   radius: %f\n",jostruct.radius);
  } else if (strcmp(jostruct.type,"plane") == 0) {
    printf("    color: [%f, %f, %f]\n",jostruct.color.x, jostruct.color.y, jostruct.color.z);
    printf(" position: [%f, %f, %f]\n",jostruct.position.x, jostruct.position.y, jostruct.position.z);
    printf("   normal: [%f, %f, %f]\n",jostruct.normal.x, jostruct.normal.y, jostruct.normal.z);
  } else if (strcmp(jostruct.type,"quadric") == 0) {
    printf("    color: [%f, %f, %f]\n",jostruct.color.x, jostruct.color.y, jostruct.color.z);
    printf(" position: [%f, %f, %f]\n",jostruct.position.x, jostruct.position.y, jostruct.position.z);
    printf("        A: %f\n",jostruct.coeffs.A);
    printf("        B: %f\n",jostruct.coeffs.B);
    printf("        C: %f\n",jostruct.coeffs.C);
    printf("        D: %f\n",jostruct.coeffs.D);
    printf("        E: %f\n",jostruct.coeffs.E);
    printf("        F: %f\n",jostruct.coeffs.F);
    printf("        G: %f\n",jostruct.coeffs.G);
    printf("        H: %f\n",jostruct.coeffs.H);
    printf("        I: %f\n",jostruct.coeffs.I);
    printf("        J: %f\n",jostruct.coeffs.J);
  } else {
    printf("Error: unrecognized type\n");
  }
  printf("\n");
}

// helper to report the results of a scene parse
void reportScene () {
  int len_array = INPUT_FILE_DATA.num_objects;
  printf("\n\n---------------------\n");
  if (INFO) message("Info","PARSE RESULTS:");
  printf("---------------------\n");
  printf("Processed scene with %d objects:\n\n",len_array);
  for (int i = 0; i < len_array; i++) {
    printJSONObjectStruct(INPUT_FILE_DATA.js_objects[i]);
  }
}

// helper to store a double onto our JSON object file
void storeDouble(int obj_count, char* key, double value) {
  if (VERBOSE) printf("   sD: storing %s,%lf at %d\n",key,value,obj_count);

  // store the actual value, not sure how to say ".key" and get it to evaluate key so need these ifs
  if (strcmp(key,"width") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].width = value;
  } else if (strcmp(key,"height") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].height = value;
  } else if (strcmp(key,"radius") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].radius = value;
  } else if (strcmp(key, "A") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.A = value;
  } else if (strcmp(key, "B") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.B = value;
  } else if (strcmp(key, "C") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.C = value;
  } else if (strcmp(key, "D") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.D = value;
  } else if (strcmp(key, "E") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.E = value;
  } else if (strcmp(key, "F") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.F = value;
  } else if (strcmp(key, "G") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.G = value;
  } else if (strcmp(key, "H") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.H = value;
  } else if (strcmp(key, "I") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.I = value;
  } else if (strcmp(key, "J") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].coeffs.J = value;
  } else {
    // This should never happen
    message("Error","Interally trying to store unknown key type");
  }
}

// helper to store a vector onto our JSON object file
void storeVector(int obj_count, char* key, double* value) {
  if (VERBOSE) printf("   sV: storing %s at %d\n",key,obj_count);
  if (strcmp(key,"color") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].color.x = value[0];
    INPUT_FILE_DATA.js_objects[obj_count].color.y = value[1];
    INPUT_FILE_DATA.js_objects[obj_count].color.z = value[2];
  } else if (strcmp(key,"position") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].position.x = value[0];
    INPUT_FILE_DATA.js_objects[obj_count].position.y = value[1];
    INPUT_FILE_DATA.js_objects[obj_count].position.z = value[2];
  } else if (strcmp(key,"normal") == 0) {
    INPUT_FILE_DATA.js_objects[obj_count].normal.x = value[0];
    INPUT_FILE_DATA.js_objects[obj_count].normal.y = value[1];
    INPUT_FILE_DATA.js_objects[obj_count].normal.z = value[2];
  } else {
    // This should never happen
    message("Error","Interally trying to store unknown vector key type");
  }

}

// Raycaster function
// builds the image based on a scene
void  rayCast(JSON_object *scene, RGBPixel *image) {

  ////////////
  // variables
  ////////////
  int intersect = 0; // dummy var while intersection tests not working
  // number of pixels that represent height/width
  int M = INPUT_FILE_DATA.height;
  int N = INPUT_FILE_DATA.width;
  int pixmap_length = M * N;
  // this represents the center of the view plane
  double cx = 0;
  double cy = 0;
  double cz = 1;
  // make a view plane according to the (first) camera object in the JSON
  double w = getCameraWidth();
  double h = getCameraHeight();
  // height/width of each pixel
  double pixwidth = w / N;
  double pixheight = h / M;
  int i = 0; // pixelmap counter, since my pixelmap is a flat array
  
  printf("Raycasting %d x %d image to memory ...\n",N,M);

  //////////////////////////////////////
  // copy of psuedo code from text/class
  //////////////////////////////////////
  for (int y = 0; y < M; y += 1) {
    for (int x = 0; x < N; x += 1) {
      // origin
      double Ro[3] = {0,0,0}; // vector that represents a point that represents the origin
      // direction
      // Rd = normalize(P - Ro), origin in 0 so skip that, but need to normalize
      // this won't work prior to C 1999 to evaluate in the static initializer
      double Rd[3] = {
	cx - (w/2) + pixwidth * ( x + 0.5),
	cy - (h/2) + pixheight * ( y + 0.5),
	cz
      };

      // next, need to make Rd so that it's actually normalized
      vNormalize(Rd);

      // structure of every ray tracer you will ever encounter
      // go over all x/y values for a scene and check for intersections
      double best_t = INFINITY;
      int    best_t_index = 129;

      for (int o = 0; o < INPUT_FILE_DATA.num_objects; o += 1) {
	//printf("DBG: o(%d) against no(%d)\n",o,INPUT_FILE_DATA.num_objects);
	// t stores if we have an intersection or not
	double t = 0;

	switch(INPUT_FILE_DATA.js_objects[o].typecode) {
	case 0:
	  //if (INFO) message("Info","Skipping camera object...");
	  break;
	case 1:
	  //if (INFO) message("Info","processing sphere...");
	  t = sphereIntersection(Ro,Rd,
				 INPUT_FILE_DATA.js_objects[o].position,
				 INPUT_FILE_DATA.js_objects[o].radius);
	  break;
	case 2:	
	  //if (INFO) message("Info","processing plane...");
	  t = planeIntersection(Ro,Rd,
				 INPUT_FILE_DATA.js_objects[o].position,
				 INPUT_FILE_DATA.js_objects[o].normal);
	  break;
	case 3:
	  //	  t = cylinder_intersection(Ro,Rd,objects[o]->cylinder.center,objects[o]->cylinder.radius);
	  t = cylinderIntersection(Ro,Rd,
				   INPUT_FILE_DATA.js_objects[o].position,
				   INPUT_FILE_DATA.js_objects[o].radius);
	  break;
	case 4:
	  t = quadricIntersection(Ro,Rd,
				   INPUT_FILE_DATA.js_objects[o].position,
				   INPUT_FILE_DATA.js_objects[o].coeffs);
	  if (VERBOSE) printf("DBG: %f\n",t);
	  break;
	default:
	  message("Error","Unhandled typecode, camera/plane/sphere are supported");
	}
	if (t > 0 && t < best_t) {
	  best_t = t;
	  best_t_index = o;
	}
      }
      // Now look at the t value and see if there was an intersection
      // remember that you could have multiple objects in from of each other, check for the smaller of the
      // t values, that hit first, color that one
      if (best_t > 0 && best_t != INFINITY) {
	if (ASCII_IMAGE) printf("#");
	RGB_PIXEL_MAP[i].r = shadePixel(scene[best_t_index].color.x);
	RGB_PIXEL_MAP[i].g = shadePixel(scene[best_t_index].color.y);
	RGB_PIXEL_MAP[i].b = shadePixel(scene[best_t_index].color.z);
      } else {
	if (ASCII_IMAGE) printf(".");
	RGB_PIXEL_MAP[i].r = shadePixel(DEFAULT_COLOR);
	RGB_PIXEL_MAP[i].g = shadePixel(DEFAULT_COLOR);
	RGB_PIXEL_MAP[i].b = shadePixel(DEFAULT_COLOR);
      }
      i++; // increment the pixelmap counter
    }
    if (ASCII_IMAGE) printf("\n");
  }
}

// helper function to convert 0 to 1 color scale into 0 to 255 color scale for PPM
unsigned char shadePixel (double value) {
  if (value > 1.0) {
    message("Error","Unsupported max color value, expected between 0 and 1.0");
  } else {
    return round(value * 255);
  }
}

/////////////////////////
// Intersection checkers
/////////////////////////
// Step 1. Find the equation for the object we are interested in
// Step 2. Parameterize the equation with a center point if needed
// Step 3. Substitute the equation for ray into our object equation
// Step 4. Solve for t.
//      4a. Rewrite the equation (flatten, get rid of parens). (maple/mathmatica will solve this, or algebra)
//      4b. rewrite the equation in terms of t, want to solve for t
// Step 5. Use the quadratic equation (if relevant) to solve for t
/////////////////////////

// Sphere intersection code (from http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter1.htm)
double sphereIntersection(double* Ro, double* Rd, x_y_z C, double r) {
  // Xhit = pr + (tclose - a)*ur from the notes, same as
  // S = the set of points[xs, ys, zs], where (xs - xc)2 + (ys - yc)2 + (zs - zc)2 = r^2
  // (Ro[0] + Rd[0] * t - C.X)2 + (Ro[1] + Rd[1] * t - C.Y)2 + (Ro[2] + Rd[2] * t - C.Z)2 = r^2
  // or A*t^2 + B*t + C = 0
  double a = sqr(Rd[0]) + sqr(Rd[1]) + sqr(Rd[2]); // with normalized ray, should always be 1
  double b = 2 * (Rd[0] * (Ro[0] - C.x) + Rd[1] * (Ro[1] - C.y) + Rd[2] * (Ro[2] - C.z));
  double c = sqr(Ro[0] - C.x) + sqr(Ro[1] - C.y) + sqr(Ro[2] - C.z) - sqr(r);

  double det = sqr(b) - 4 * a * c;
  if (det < 0) return -1;

  // take the square root
  det = sqrt(det);
  
  //t0, t1 = (- B + (B^2 - 4*C)^1/2) / 2 where t0 is for (-) and t1 is for (+)
  // compute t0, if positive, this is the smaller of 2 intersections, we are done, return it
  double t0 = (-b - det) / (2 * a);
  if (t0 > 0) return t0;

  double t1 = (-b + det) / (2 * a);
  if (t1 > 0) return t1;

  // no intersection if we fall through to here
  return -1;
}

// Sphere intersection code (from notes)
// arguments are: the ray (origin/direction), center of the plane, normal of the plan
// TODO: apparently not working properly
double planeIntersection(double* Ro, double* Rd, x_y_z C, x_y_z N) {
  // t = (n dot (Pr - Po))/(n dot Ur) or, from siggraph:
  // Ax + By + Cz + D = 0, which becomes the following after being centered and plugging in ray equation
  // t = -(AX0 + BY0 + CZ0 + D) / (AXd + BYd + CZd)
  // t = -(Pn� R0 + D) / (Pn � Rd)

  // the projection of Vo onto a vector from the origin to shortest point on plane

  // Convert some values from x_y_z struct format to V3 format for 3d math
  double n[3];
  double c[3];
  convertXYZ(N,n);
  convertXYZ(C,c);

  // if Vd = (Pn � Rd) = 0, no intersection, so compute it first and return if no intersection
  double Vd = vDot(n,Rd);
  if (Vd == 0) return -1;
  //if (Vd > 0) return -1; // TODO determine this case

  double origin[3] = {0, 0, 0}; // always assume this for origin
  // distance is a scalar (sqrt(sqr(x),...)
  // dot product of n,Ro is a scalar, so that addition is standar addition
  double d;
  if (VERBOSE) printf("c0: %f, o0: %f, c1: %f, o1: %f, c2: %f, o2: %f\n",c[0],origin[0],c[1],origin[1],c[2],origin[2]);
  //  d = sqrt(sqr(c[0] + origin[0]) + sqr(c[1] + origin[1]) + sqr(c[2] + origin[2]));
  d = vDistToPlane(origin,C,N);
  double V0 = -(vDot(n,Ro) + d);
  double t = V0 / d;
  if (VERBOSE) printf("Vd: %f, d: %f, t: %f\n",Vd,d,t);

  if (t > 0) return -1; // plane intersection is behind origin, ignore it

  // if we got this far, plane intersects the ray in front of origin and faces the ray
  // compute the intersection point: Pi = [Xi Yi Zi] = [X0 + Xd * t Y0 + Yd * t Z0 + Zd * t]
  // actually, who cares about the intersection we have distance t along the ray, so we are good
  if (VERBOSE) printf("returning %f\n",t);
  return t;
  /*
// dot product (3D) which  allows vector operations in arguments
#define dot(u,v)   ((u).x * (v).x + (u).y * (v).y + (u).z * (v).z)
#define norm(v)    sqrt(dot(v,v))  // norm = length of  vector
#define d(P,Q)     norm(P-Q)        // distance = norm of difference
  */
}

// Cylinder intersection code (from example in class) 
double cylinderIntersection(double* Ro, double* Rd, x_y_z C, double r) {
  // x^2 + z^2 = r^2   using z instead of y will make it go up/down, instead of looking head on
  double a = (sqr(Rd[0]) + sqr(Rd[2])); // remember that x/y/z are in array form
  double b = (2 * (Ro[0] * Rd[0] - Rd[0] * C.x + Ro[2] * Rd[2] - Rd[2] * C.z));
  double c = sqr(Ro[0]) - 2*Ro[0] * C.x + sqr(C.x) + sqr(Ro[2]) - 2*Ro[2] * C.z + sqr(C.z) - sqr(r);

  // determinant, remember the negative version is not real, imaginary, not 
  double det = sqr(b) - 4 * a * c;
  if (det < 0 ) return -1; // this is the signal that there was not an intersection

  // since we are using it more than once
  det = sqrt(det);
  
  double t0 = (-b - det) / (2*a);
  if (t0 > 0) return t0; // smaller/lesser of the 2 values, needs to come first

  double t1 = (-b + det) / (2*a);
  if (t1 > 0) return t1;

  // this is a default case if we have no intersection, could also just return t1, but this accounts
  // for "numeric stability" as numbers become very close to 0
  return -1;
}

// Quadric intersection code (from siggraph)
/*
double quadricIntersection(double* Ro, double* Rd, x_y_z C, A_J c) {
  return -1;
}
*/
double quadricIntersection(double* Ro, double* Rd, x_y_z C, A_J c) {
  /*
  // From the siggraph page directly, no ability to center the quadric
  double Aq = c.A * sqr(Rd[0]) + c.B * sqr(Rd[1]) + c.C * sqr(Rd[2]) +
    c.D * Rd[0] * Rd[1] + c.E * Rd[0] * Rd[2] + c.F * Rd[1] * Rd[2];
  double Bq = (2 * c.A * Ro[0] * Rd[0]) + (2 * c.B * Ro[1] * Rd[1]) +
    (2 * c.C * Ro[2] * Rd[2]) + c.D * (Ro[0] * Rd[1] + Ro[1] * Rd[0]) +
    (c.E * Ro[0] * Rd[2]) + c.F * (Ro[1] * Rd[2] + Rd[1] * Ro[2]) +
    c.G * Rd[0] + c.H * Rd[1] + c.I * Rd[2]; // pr> at end?
  double Cq = c.A * sqr(Ro[0]) + c.B * sqr(Ro[1]) + c.C * sqr(Ro[2]) +
    c.D * Ro[0] * Ro[1] + c.E * Ro[0] * Ro[2] + c.F * Ro[1] * Ro[2] +
    c.G * Ro[0] + c.H * Ro[1] + c.I * Ro[2] + c.J ;//should always be just J, since our origin is 0,0,0
  */

  //TODO: need to add in C (center/position offset?)
  // Try with centered coords:
  // Ax^2 + By^2 + Cz^2 + Dxy+ Exz + Fyz + Gx + Hy + Iz + J = 0
  // A(x + cx)^2 + B(y + cy)^2 + C(z + cz)^2 + D(x + cx)(y + cy) +
  // E(x + cx)(z + cz) + F(y + cy)(z + cz) + G(x + cx) + H(y + cy) +
  // I(z + cz) + J = 0
  //
  // On one line:
  // A(x + cx)^2 + B(y + cy)^2 + C(z + cz)^2 + D(x + cx)(y + cy) + E(x + cx)(z + cz) + F(y + cy)(z + cz) + G(x + cx) + H(y + cy) + I(z + cz) + J = 0
  //
  // Now substitue in the ray and solve for t (assume Ro component is always 0, so skip it :
  // A(t*Rdx + cx)^2 + B(t*Rdy + cy)^2 + C(t*Rdz + cz)^2 + D(t*Rdx + cx)(t*Rdy + cy) + E(t*Rdx + cx)(t*Rdz + cz) + F(t*Rdy + cy)(t*Rdz + cz) + G(t*Rdx + cx) + H(t*Rdy + cy) + I(t*Rdz + cz) + J = 0
  //
  // 
  VERBOSE = 0;
  if (VERBOSE) printf("DBG : xyz=(%f,%f,%f) AJ=(%f,%f,%f,%f)\n",C.x,C.y,C.z,c.A,c.B,c.C,c.D);
  if (VERBOSE) printf("DBG : Rd's > x=%f, y=%f, z=%f\n",Rd[0],Rd[1],Rd[2]);
  double Aq = c.A*sqr(Rd[0]) + c.B*sqr(Rd[1]) + c.C*sqr(Rd[2]) + c.D*Rd[0]*Rd[1] + c.E*Rd[0]*Rd[2] + c.F*Rd[1]*Rd[2];
  double Bq = - 2*c.A*Rd[0]*C.x - 2*c.B*Rd[1]*C.y - 2*c.C*Rd[2]*C.z - c.D*Rd[0]*C.y - c.D*Rd[1]*C.x -
    c.E*Rd[0]*C.z - c.E*Rd[2]*C.x - c.F*Rd[1]*C.z - c.F*Rd[2]*C.y + c.G*Rd[0] + c.H*Rd[1] + c.I*Rd[2];
  double Cq = c.A*sqr(C.x) + c.B*sqr(C.y) + c.C*sqr(C.z) + c.D*C.x*C.y +
    c.E*C.x*C.z + c.F*C.y*C.z - c.G*C.x - c.H*C.y - c.I*C.z + c.J;
  if (VERBOSE) printf("DBG : Aq=%f, Bq=%f, Cq=%f)\n",Aq,Bq,Cq);
    
  
  // 1. Check Aq = 0 (If Aq = 0 then t = -Cq / Bq
  // 2.If Aq � 0, then check the discriminant (If Bq2 - 4AqCq < 0 then there is no intersection)
  // 3. Compute t0 and if t0 > 0 then done else compute t1
  if (Aq == 0) return -Cq / Bq;
  
  // discriminant 
  //double disc = (sqr(Bq) - (4 * Aq * Cq)) * -1;
  double disc = (sqr(Bq) - (4 * Aq * Cq));
  if (VERBOSE) printf("DBG : disc=(%f), where Bq^2=(%f) and 4ac=(%f)\n",disc,sqr(Bq),(4*Aq*Cq));
  //if (disc < 0) return -1; // TODO: is this true for quadrics?
  
  // since we are using it more than once
  disc = sqrt(disc) ;
  
  double t0 = (-Bq - disc) / (2 * Aq);
  if (VERBOSE) printf("DBG : t0=%f\n",t0);
  if (t0 > 0) return t0; // smaller/lesser of the 2 values, needs to come first

  double t1 = (-Bq + disc) / (2 * Aq);
  if (VERBOSE) printf("DBG : t1=%f\n",t1);
  if (t1 > 0) return t1;

  return -1;
}

// Helper functions to find the first camera object and get it's specified width/height
double getCameraWidth() {
  double w = 0.0;
  for (int o = 0; o < INPUT_FILE_DATA.num_objects; o++) {
    if (INPUT_FILE_DATA.js_objects[o].typecode == 0) {
      w = INPUT_FILE_DATA.js_objects[o].width;
      if (w > 0) {
	printf("Info: Found camera object width %f\n",w);
	return w;
      } else {
	message("Error","Unsupported camera width less than zero");
      }
    }
  }
  return w;
}
double getCameraHeight() {
  double h = 0.0;
  for (int o = 0; o < INPUT_FILE_DATA.num_objects; o++) {
    if (INPUT_FILE_DATA.js_objects[o].typecode == 0) {
      h = INPUT_FILE_DATA.js_objects[o].height;
      if (h > 0) {
	printf("Info: Found camera object height %f\n",h);
	return h;
      } else {
	message("Error","Unsupported camera height less than zero");
      }
    }
  }
  return h;
}

// helper function for JSON error checking (like does a sphere have a width, etc...)
// TODO: move this error checking into the parser
void checkJSON (JSON_object *object) {
  if (INFO) message("Info","Checking JSON for errors...");
  // variables
  
  // code body
  // TODO: finish error checking once it's known how to check for existence
  for (int o = 0; o < INPUT_FILE_DATA.num_objects; o++) {
    switch(object[o].typecode) {
    case 0: // camera
      if (!object[o].width || !object[o].height)
	message("Error","Camera object must have width and height properties");
      //      if (object[o].radius || sizeof(object[o].normal) > 0 || sizeof(object[o].color) > 0)
      if (object[o].radius)
	if (INFO) message("Info","Ignoring camera object properties in excess of width/height");
      break;
    case 1: // sphere
      //      if (!object[o].radius || !(object[o].position.x > sizeof(object[o].position)) || !object[o].color.x)
      if (!object[o].radius)      
	message("Error","Sphere object is missing radius, position, or color");
      if (object[o].width || object[o].height || sizeof(object[o].normal) > 0)
	if (INFO) message("Info","Ignoring sphere object properties in excess of radius/position/color");
      break;
    case 2: // plane
      /*
      if (sizeof(object[o].normal) == 0 || !object[o].position || !object[o].color)
	message("Error","Plane object is missing normal, position, or color");
      if (object[o].width || object[o].height || object[o].radius)
	if (INFO) message("Info","Ignoring plane object properties in excess of normal/position/color");
      */
      break;
    case 3: // cylinder
      break;
    case 4:
      break;
    default:
      message("Error","Un-caught error, was missed during parsing");
    }
  }
  if (INFO) message("Info","Done checking JSON for errors...");
}
  
