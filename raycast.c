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

---------------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
  int lines;
  int width;
  int height;
  int alpha;
  int depth;
  char *tupltype;
  FILE* fh_in;
} PPM_file_struct ;

typedef struct JSON_file_struct {
  char *type;
  double width;
  double height;
  double *color;
  double *position;
  double *normal;
  double radius;
  FILE* fh_in;
} JSON_file_struct ;

// global variables
int CURRENT_CHAR        = 'a';
int OUTPUT_MAGIC_NUMBER = 6; // default to P6 PPM format
int VERBOSE             = 0; // controls logfile message level

// global data structures
JSON_file_struct    INPUT_FILE_DATA;
RGBPixel           *RGB_PIXEL_MAP;
RGBAPixel          *RGBA_PIXEL_MAP;
PPM_file_struct     OUTPUT_FILE_DATA;

// functions
void  skipWhitespace(FILE* json);
char* getString(FILE* json);
void  parseJSON(char* input_file);
void  writePPM        (char *outfile,         PPM_file_struct *input);
void  message         (char message_code[],   char message[]        );
void  writePPMHeader  (FILE* fh              );
void  reportPPMStruct (PPM_file_struct *input);
void  reportPixelMap  (RGBPixel *pm          );

void  help            ();
int   computeDepth();
char* computeTuplType();
void  freeGlobalMemory ();
void  closeAndExit ();

/*
 ------------------------------------------------------------------
                                 MAIN
 ------------------------------------------------------------------
*/
int main(int argc, char *argv[]) {
  //message("Usage","raycast width height input.json output.ppm");
  if (argc != 5) {
    help();
    return(1);
  }

  int width = argv[1];
  int height = argv[2];
  char *infile = argv[3];
  char *outfile = argv[4];

  message("Info","Processing the following arguments:");
  printf("          Input : %s\n",infile);
  printf("          Output: %s\n",outfile);
  printf("          Width : %d\n",width);
  printf("          Height: %d\n",height);

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
// "separation of concerns" means we should pass data in, not work on global values
void skipWhitespace(FILE* json) {
  int c = fgetc(json);
  while (isspace(c)) {
    c = fgetc(json);
  }
  // give the first non-whitespace char back
  ungetc(c,json);
}

// helper to get a string from current position
// can assume no space in the string itself
char* getString(FILE* json) {
  char buffer[128]; // since strdup will give correct size, could also start smaller and realloc()
  int c = fgetc(json);
  // error checking
  if (c != '"') {
    fprintf(stderr, "Error: unexpected string\n"); // could keep line number as well to impress
    exit(1);
  }

  c = fgetc(json); // should be first char in string unless empty string
  int i = 0;
  while (c != '"') {
    buffer[i] = c;
    c = fgetc(json);
    i++;
  }
  buffer[i] = 0; //terminator

  //return buffer; // can't just return it, 
  return strdup(buffer);
}

void parseJSON(char* input_file) {
  // plain text, so no need for binary mode
  FILE* json = fopen(input_file,"r");

  // now read the data
  // skip whitespace until open bracket (he'll put it in there just to trip us up)
  skipWhitespace(json);
  int c = fgetc(json);
  // beginning of list
  if (c != '[') {
    fprintf(stderr,"Error: File must begin with [\n");
    fclose(json); // any modern OS should do this, but should write the cleanup proc to free also
    exit(1);
  }
  skipWhitespace(json);

  // find the objects
  c = fgetc(json);
  // handle empty lists can also be valid
  if (c == ']') {
    fclose(json);
    exit(0); // actually, we'll have to do the raycaster part here
  }
  if (c != '"' ) {
    // parse the object, we know type is first which is nice
    char* key = getString(json);
    if (strcmp(key,"type") != 0) {
      exit(1);
    }
    skipWhitespace(json);
    
    c = fgetc(json);
    if (c != ':') {
      exit(1);
    }

    skipWhitespace(json);
    
    char* type = getString(json);
    if (strcmp(type, "camera") == 0) {
    } else if (strcmp(type, "sphere") == 0) {
    } else if (strcmp(type, "plane") == 0) {
    } else {
      // unknown type
      exit(1);
    }
  }
  
  
  // make sure to close so the buffer gets written, defined behavior
  fclose(json);
}

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
  //  free(RGBA_PIXEL_MAP);
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
