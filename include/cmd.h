#include "iostream"
using namespace std;

class Command{
private:
   string str;                   //input string
   int count_arg,count_file;
   string cmd_name;              //command name
   string *cmd_arg;              //optional arguments
   string *cmd_file;            //file or directory names

public:
   Command();
   Command(string);
   ~Command();

   void analyze();

   string get_str();

   string get_name();
   string get_arg(int);
   string get_file(int);

   void print_arg();
   void print_file();

   int get_arg_no();
   int get_file_no();

   bool find_arg(string);
   bool find_file(string);
};

