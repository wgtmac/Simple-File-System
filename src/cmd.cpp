#include "../include/cmd.h"

   Command::Command():str(""),count_arg(0),count_file(0),cmd_name(""){
        cmd_file = new string[10];      
        cmd_arg = new string [10];
   }  //default 
   Command::Command(string s):str(s),count_arg(0),count_file(0),cmd_name(""){
        cmd_file = new string[10];      
        cmd_arg = new string [10];
   }
   Command::~Command(){
        delete []cmd_file;
        delete []cmd_arg;
   }

   void Command::analyze(){
      bool flag = false,find = false;
      short start,end;
 
      for(size_t i = 0; i<str.size(); i++){
          if(!flag){
             if( str[i+1] == ' ' || str[i+1] == '\0'){
                string tmp(str,0,i+1);
                cmd_name = tmp;
                flag = true;
             }
          }
          else if(str[i] != ' '){
             if(str[i-1] == ' ')                  //find a header
                 start = i;  
             if(str[i+1] == ' ' || str[i+1] == '\0' ){
                 end = i;
                 find = true;
             }
             if(find == true){
                 if(str[start] == '-'){             //arg
                     string tmp(str,start,end-start+1);
                     cmd_arg[count_arg++] = tmp;
                 }

                 else{                               //file or dir
                     string tmp(str,start,end-start+1);
                     cmd_file[count_file++] = tmp;
                 }
                 find = false;
             }
          }

      }//end for
   }//end analyze_cmd

   string Command::get_str(){
      return str;
   }

   string Command::get_name(){
      return cmd_name;
   }

   string Command::get_arg(int i){
      if(i<0 && i>count_arg)return "";
      else return cmd_arg[i];
   }
   string Command::get_file(int i){
      if(i<0 && i>count_file)return "";
      else return cmd_file[i];
   }

   void Command::print_file(){
      if(count_file){
        cout<<"File or dir list :";
        for(size_t i=0;i<count_file;i++)
            cout<<cmd_file[i]<<" ";
        cout<<endl;
      }
   }

   void Command::print_arg(){
      if(count_arg){
        cout<<"Argument list :";
        for(size_t i=0;i<count_arg;i++)
            cout<<cmd_arg[i]<<" ";
        cout<<endl;
      }
   }

   int Command::get_arg_no(){
      return count_arg;
   }

   int Command::get_file_no(){
      return count_file;
   }

   bool Command::find_arg(string tmp){
      bool flag = false;
      for(size_t i=0;i<count_arg;i++)
          if(cmd_arg[i]==tmp)flag=true;
      return flag;
   }

     bool Command::find_file(string tmp){
      bool flag = false;
      for(size_t i=0;i<count_file;i++)
          if(cmd_file[i]==tmp)flag=true;
      return flag;
   }

