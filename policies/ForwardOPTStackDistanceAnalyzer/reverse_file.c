#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "USAGE: ./reverse_file inputFileName outputFileName\n");
    exit(1);
  }
  
  char *input_file_name = argv[1];
  char *output_file_name = argv[2];
  FILE * input_file = fopen(input_file_name, "r");
  FILE * output_file = fopen(output_file_name, "w");

  fseek(input_file, 0, SEEK_END);
  long int input_file_size = ftell(input_file);
  //fprintf(stderr, "input file size: %d\n", input_file_size);

  long int buf_size = 10;
  long int offset = -buf_size;
  char buffer[buf_size];
  long int buf_pos;
  char line[256];
  long int line_pos = 0;
  long int total = 0;

  while (total < input_file_size) {
    //fprintf(stderr, "--------\n");
    fseek(input_file, offset, SEEK_END);
    long int real_length = fread (buffer, 1, buf_size, input_file);
    buf_pos = real_length - 1;
    //fprintf(stderr, "real length: %d\n", real_length);
    total += real_length;
    //fprintf(stderr, "total: %d\n", total);

    /*
    int i;
    for (i=0;i<real_length;i++)
      fprintf(stderr, "%c", buffer[i]);
    fprintf(stderr, "\n");
    */

    while (buf_pos >= 0) {
      //fprintf(stderr, "%c", buffer[buf_pos]);
      //fprintf(stderr, "\t buf_pos=%d\n", buf_pos);
      line[line_pos++] = buffer[buf_pos--];

      char line2[256];
      int i,j;
      if (line[line_pos-1] == '\n') {
	for (i=line_pos-2,j=0;i>=0;--i,++j) {
	  line2[j] = line[i];
	}
	if (j != 0) {
	  line2[j++] = '\n';
	  line2[j] = '\0';
	  //fprintf(stderr, "line2: %s\n", line2);
	  fputs(line2, output_file);
	}
	line_pos = 0;
      } else if ((buf_pos < 0) && (total == input_file_size)) {
	for (i=line_pos-1,j=0;i>=0;--i,++j) {
	  line2[j] = line[i];
	}
	line2[j++] = '\n';
	line2[j] = '\0';
	//fprintf(stderr, "line2: %s\n", line2);
	fputs(line2, output_file);
      }
    }

    offset = offset - buf_size;
    if (offset+input_file_size < 0) {
      buf_size = buf_size + offset + input_file_size;
      offset = -input_file_size;
    }
  }

  fclose(input_file);
  fclose(output_file);
  return 0;

}
