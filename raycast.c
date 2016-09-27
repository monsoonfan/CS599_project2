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

Questions:
---------
- share emacs tricks: match-paren, cursor point/jump, ctrk-k, goto-line
- can we support 1 and only 1 camera?
- positioning of the objects themselves, relative to what?
- how to pass a parameterized member: 	    INPUT_FILE_DATA.js_objects[obj_count].&key = value;
- can we include 3dmath.h, or does that need to be inline?
---------------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


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

typedef struct JSON_object {
  char *type; // helpful to have both string and number reference for this
  int  typecode; // 0 = camera, 1 = sphere, 2 = plane
  double width;
  double height;
  x_y_z color;
  x_y_z position;
  x_y_z normal;
  double center[3];
  double radius;
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

// inline functions:
static inline double sqr (double v) {
  return v * v;
}

static inline void normalize (double* v) {
  double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}
// END inline functions

// global variables
int CURRENT_CHAR        = 'a';
int OUTPUT_MAGIC_NUMBER = 6; // default to P6 PPM format
int VERBOSE             = 0; // controls logfile message level

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
void  printJSONObjectStruct (JSON_object jostruct);
void  storeDouble(int obj_count, char* key, double value);
void  storeVector(int obj_count, char* key, double* value);
void  rayCast(JSON_object *scene, RGBPixel *image);
double cylinderIntersection(double* Ro, double* Rd, x_y_z C, double r);
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
	message("Info","Processing camera object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "camera";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 0;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "sphere") == 0) {
	message("Info","Processing sphere object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "sphere";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 1;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "plane") == 0) {
	message("Info","Processing plane object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "plane";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 2;
	INPUT_FILE_DATA.num_objects = obj_count + 1;
      } else if (strcmp(value, "cylinder") == 0) {
	message("Info","Processing cylinder object...");
	INPUT_FILE_DATA.js_objects[obj_count].type = "cylinder";
	INPUT_FILE_DATA.js_objects[obj_count].typecode = 3;
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
	      (strcmp(key, "radius") == 0)) {
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
  message("Info","Read scene file");
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
  
  message("Info","Processing the following arguments:");
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
  message("Info","Done writing header");
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
    message("Info","Outputting format 3");
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
    message("Info","Outputting format 6");
    fwrite(RGB_PIXEL_MAP, sizeof(RGBPixel), OUTPUT_FILE_DATA.width * OUTPUT_FILE_DATA.height, fh_out);
    break;
    // P7 format
  case(7):
    // write the entire pixel_map in one command, RGB writes from RGB pixel_map and RGBA writes from RGBA pixel_map
    message("Info","Outputting format 7");
    if (strcmp(OUTPUT_FILE_DATA.tupltype,"RGB_ALPHA") == 0) {
      message("Info","   output file will have alpha data");
      fwrite(RGBA_PIXEL_MAP, sizeof(RGBAPixel), OUTPUT_FILE_DATA.width * OUTPUT_FILE_DATA.height, fh_out);
    } else {
      message("Info","   output file is RGB only");
      fwrite(RGB_PIXEL_MAP, sizeof(RGBPixel), OUTPUT_FILE_DATA.width * OUTPUT_FILE_DATA.height, fh_out);
    }
    break;
  default:
    message("Error","Unrecognized output format");
  }
  // ---------------------- done write image ---------------------------------

  fclose(fh_out);
  reportPPMStruct(&OUTPUT_FILE_DATA);
  message("Info","Done ");
}

// helper function to visualize what's in a given PPM struct
void reportPPMStruct (PPM_file_struct *input) {
  message("Info","Contents of PPM struct:");
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
  } else {
    printf("Error: unrecognized type\n");
  }
  printf("\n");
}

// helper to report the results of a scene parse
void reportScene () {
  int len_array = INPUT_FILE_DATA.num_objects;
  printf("\n\n---------------------\n");
  message("Info","PARSE RESULTS:");
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
	1
      };

      // next, need to make Rd so that it's actually normalized
      normalize(Rd);

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
	  //	  message("Info","Skipping camera object...");
	  break;
	case 1:
	  //message("Info","processing sphere...");
	  break;
	case 2:	
	  //message("Info","processing plane...");
	  break;
	case 3:
	  //	  t = cylinder_intersection(Ro,Rd,objects[o]->cylinder.center,objects[o]->cylinder.radius);
	  t = cylinderIntersection(Ro,Rd,
				   INPUT_FILE_DATA.js_objects[o].position,
				   INPUT_FILE_DATA.js_objects[o].radius);
	  break;
	default:
	  // horrible error
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
	printf("#");
	RGB_PIXEL_MAP[i].r = shadePixel(scene[best_t_index].color.x);
	RGB_PIXEL_MAP[i].g = shadePixel(scene[best_t_index].color.y);
	RGB_PIXEL_MAP[i].b = shadePixel(scene[best_t_index].color.z);
      } else {
	printf(".");
	RGB_PIXEL_MAP[i].r = shadePixel(1);
	RGB_PIXEL_MAP[i].g = shadePixel(1);
	RGB_PIXEL_MAP[i].b = shadePixel(1);
      }
      i++; // increment the pixelmap counter
    }
    printf("\n");
  }



  /*
  // loop over each pixel, do this in a single loop as opposed to x and y since pixmap is flat
  for (int i = 0; i < pixmap_length; i++) {
    // loop over each object
    for (int j = 0; j < INPUT_FILE_DATA.num_objects; j++) {
      // intersection test for all objects?
      if (intersect) {
	RGB_PIXEL_MAP[i].r = shadePixel(scene[j].color.x);
	RGB_PIXEL_MAP[i].g = shadePixel(scene[j].color.y);
	RGB_PIXEL_MAP[i].b = shadePixel(scene[j].color.z);
      } else {
	RGB_PIXEL_MAP[i].r = shadePixel(1);
	RGB_PIXEL_MAP[i].g = shadePixel(1);
	RGB_PIXEL_MAP[i].b = shadePixel(1);
      }
    }
  }
  */
  
  message("TODO","Build this thing!");
}

// helper function to convert 0 to 1 color scale into 0 to 255 color scale for PPM
unsigned char shadePixel (double value) {
  if (value > 1.0) {
    message("Error","Unsupported max color value, expected between 0 and 1.0");
  } else {
    return round(value * 255);
  }
}

// Cylinder intersection code (from example in class) 
double cylinderIntersection(double* Ro, double* Rd, x_y_z C, double r) {
  // Step 1. Find the equation for the object we are interested in
  // Step 2. Parameterize the equation with a center point if needed
  // Step 3. Substitute the equation for ray into our object equation
  // Step 4. Solve for t.
  //      4a. Rewrite the equation (flatten, get rid of parens). (maple/mathmatica will solve this, or algebra)
  //      4b. rewrite the equation in terms of t, want to solve for t
  // Step 5. Use the quadratic equation to solve for t
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

/*
TODO:
error checking (like does a sphere have a width, etc...)

pixel to origin is the ray
*/
