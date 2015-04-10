/*
磁盘块每块4KB 第一个磁盘块作为指示用 
然后
*/
#include "math.h"
#include "time.h"


#define TABLE_LENGTH 100     //磁盘块数
#define BLOCK_LENGTH TABLE_LENGTH  
#define INDEX_LENGTH 10     //索引盘块数
#define DATA_LENGTH BLOCK_LENGTH-INDEX_LENGTH-1    //数据域盘块数
#define BLOCK_SIZE 4*1024
#define INDEX_NUM BLOCK_SIZE/128
#define INC_FILE 27            //文件夹包含文件上限
#define LINE_LENGTH 32     //文件每行宽度   


#define UINT_SIZE sizeof(unsigned int)
#define CHAR_SIZE sizeof(char)


typedef struct Table{              //磁盘负载表
	char state[TABLE_LENGTH];                                          //指示每个磁盘块是否空闲 空闲为0 大小为TABLE_LENGTH Byte
	char static_place[BLOCK_SIZE-TABLE_LENGTH];           //占位用 填满4KB
}TABLE;
#define TABLE_SIZE sizeof(struct Table)

typedef struct FNode{             //索引  大小128 Byte   一个磁盘块可以存32个文件/文件夹
    short id;                              // 文件or文件夹号   小于0文件夹   大于0文件  等于0未使用  计算公式 索引盘块号×索引盘块数+（序号+1）  再取正负号
	char name[61];                   //文件名或者文件夹名 限长60个字符
	ushort start_block;             //文件对应首数据区磁盘块号 文件夹则此项无效
	short back;                        //上级目录号  没有则为0
	short next[INC_FILE];         //若是文件夹则此项为内含文件号 不含则为0  最多含27个文件
	uchar state;                       //文件读写状态   xx------ 分别代表   【  x  x （授权者）读 写 移动  （root）读 写 移动 】   （目录为dx------）
	uchar userid;                      //使用者id   前四位为group 可以从0～15  后四位为user 可以从0～15  0代表root：root
	time_t time;
}FNODE;
#define FNODE_SIZE sizeof(struct FNode)

typedef struct Index{              //磁盘块索引   4096KB
	FNODE fnode[INDEX_NUM];               //文件索引
}INDEX;
#define INDEX_SIZE sizeof(struct Index)

typedef struct File{
	int size;                               //文件大小
	ushort start;                       //起始盘块
	uchar total;                        //共占有盘块数 
	char Data[BLOCK_SIZE-32];                //数据域 多余的写在相邻磁盘上
	char unused[32-8];
	//创建时间，最近修改时间，上次使用时间暂时不写 
}FILETYPE;
#define FILE_SIZE sizeof(struct File)

typedef union Block{       //对应存储数据用磁盘块
	FILETYPE file;
	char Data[BLOCK_SIZE];
}BLOCK;
#define BLOCKSIZE sizeof(BLOCK)

typedef struct Disk{
	TABLE d_table;                                               //磁盘负载表用磁盘块
	INDEX d_index[INDEX_LENGTH];                   //索引区磁盘块
	BLOCK d_block[DATA_LENGTH];                   //数据区磁盘块 
}DISK;
#define DISK_SIZE sizeof(DISK)


/****************************************************************************/
/*********************************basic function****************************/
/****************************************************************************/

void initialization(DISK& disk){
	for(size_t i=0;i<BLOCK_LENGTH;i++)
		i>=( BLOCK_LENGTH-INDEX_LENGTH-1)?disk.d_table.state[i] = 1:disk.d_table.state[i] = 0;
	for(size_t i=0;i<INDEX_LENGTH;i++)
		for(size_t j=0;j<INDEX_NUM;j++)
			disk.d_index[i].fnode[j].id = 0;                            //初始化所有索引节点为未使用
}


bool get_index(short no, size_t& i, size_t& j){
	if(no!=0){
		i = (-no-1)/32;
		j = (-no-1)%32;
		return true;
	}
	else return false;
}

bool get_index_2(short no, size_t& i, size_t& j){
	if(no!=0){
		i = (no-1)/32;
		j = (no-1)%32;
		return true;
	}
	else return false;
}

bool seperate(string str,string sep_str[], int& no){          //拆分带 /的文件名or文件夹名
	uchar num = 0;;
	if(str[0]!='/')return false;        //第一个不是/
	for(size_t i = 0; i<str.size(); i++)
		if(str[i]=='/')num++;

	for(size_t i = 1; i<str.size(); i++){
		if(str[i]=='/' && str[i-1]=='/'){
			return false;
		}
	}

	short start,end;
	bool find = false;
	int k = 0;
     for(size_t i = 1; i<str.size(); i++){
		 if(str[i-1] == '/')                  //find a header
			 start = i;  
		 if(str[i+1] == '/' || str[i+1] == '\0' ){
			 end = i;
			 find = true;
		 }
		 if(find == true){
			 string tmp(str,start,end-start+1);
			 sep_str[k++] = tmp;
			 find = false;
		 }
	 }
	 no = k;
	 return true;
}



/*****************************************************************/
/*******************************cd******************************/
/*****************************************************************/

bool cd(DISK& disk, string name , short &back_folder , uchar user = 0, char flag = 0){          //比较name长度   不含路径的/ flag = 0表示错误要输出
	bool find =false;            //     cout<<back_folder<<" hdsadhsaflhdsadagag"<<endl;/////////////
	size_t i, j, t, m , n;      //索引盘块号×索引盘块数+（序号+1）  再取正负号
	i = (-back_folder-1)/32;
	j = (-back_folder-1)%32;              //算得当前文件夹在哪
	if(back_folder<0){             //当前有目录
		//cout<<1<<endl;
		const char *str_tmp = name.c_str();
		for(t=0;t< INC_FILE ;t++){
			if(disk.d_index[i].fnode[j].next[t]<0){           //查找当前文件夹下有没有要打开的目录
				m = (-disk.d_index[i].fnode[j].next[t]-1)/32;
				n = (-disk.d_index[i].fnode[j].next[t]-1)%32;   
				if(strcmp(disk.d_index[m].fnode[n].name , str_tmp)==0){      //找到同名的
					find = true;
					back_folder = disk.d_index[i].fnode[j].next[t];
					break;
				}
			}
		}
	}
	else if(back_folder==0){                   //当前是最外层
	//	cout<<2<<endl;
		const char *str_tmp = name.c_str();
		for(i=0;i<INDEX_LENGTH;i++){
			for(j=0;j<INDEX_NUM;j++){
				if(disk.d_index[i].fnode[j].id<0 && strcmp(disk.d_index[i].fnode[j].name,str_tmp)==0 && disk.d_index[i].fnode[j].back==0){          //找到一个同名的
					find = true;
					back_folder = disk.d_index[i].fnode[j].id; 
					break;
				}
			}
			if(find==true)break;
		}
	}
	if(!find){
	    if(flag ==0)cout<<"Can not find directory :"<<name<<" !"<<endl;
		return false;
	}
	return true;
}


bool cd_back(DISK& disk, short &back_folder , uchar user = 0){
	if(back_folder==0)return false;
	else {
		size_t i,j;      //索引盘块号×索引盘块数+（序号+1）  再取正负号
		i = (-back_folder-1)/32;
		j = (-back_folder-1)%32;
		back_folder = disk.d_index[i].fnode[j].back;
		return true;
	}
}

bool cd_2(DISK& disk, string name , short &back_folder , uchar user = 0){
	string tmp[10]; 
	short temp = back_folder;
	back_folder = 0;
	int num;
	if(!seperate(name,tmp, num)){       //切割
		cout<<"Wrong path !"<<endl;
		return false;
	}
	for(size_t k=0;k<num;k++){
		if(cd(disk,tmp[k],back_folder ,user,1)){           //如果目录已经存在 则进入
			continue;
		}
		else{
			cout<<"directory "<<tmp[k]<<" doesn't exist!"<<endl;
			back_folder = temp;
			return false;
		}
	}
	return true;
}

bool test_cd(Command& cmd, DISK& disk,short& back_folder , uchar user = 0){ 
	  if(cmd.get_name()=="cd"){       
	 // short temp =  back_folder;
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_arg_no())FLAG_CMD |= 0x01;        //参数表不为0
	  if(cmd.get_file_no()==1)FLAG_CMD |= 0x02;   //文件列表参数=1
	  if(cmd.get_file_no()>1)FLAG_CMD != 0x10;
	  if(cmd.find_file(".."))FLAG_CMD |= 0x04;        //有".."
	 // cout<<(int)FLAG_CMD<<endl;
      switch(FLAG_CMD){
		  case 0:cout<<"Need more arguments !"<<endl; break;
		  case 2:
			  { 
				  string tmp = cmd.get_file(0);
				  if(tmp[0]=='/'){
					  //back_folder = 0;
					  cd_2(disk,cmd.get_file(0),back_folder,user); 
				  }
				  else 
					  cd(disk,cmd.get_file(0),back_folder,user);  
				  break;
			  }
          case 6: cd_back(disk,back_folder,user);  break;
          default:cout<<"Incorrect format !"<<endl; break; return false;
      }
      return true;
   }
   else return false;
}

/****************************************************************************/
/**************************************ls************************************/
/****************************************************************************/

void ls(DISK& disk , short back_folder , uchar user = 0){
	for(size_t i=0;i<INDEX_LENGTH;i++)
		for(size_t j=0;j<INDEX_NUM;j++){
			if(disk.d_index[i].fnode[j].id != 0 && disk.d_index[i].fnode[j].back ==back_folder){
					cout<<disk.d_index[i].fnode[j].name<<" ";
			}         
               }
        cout<<endl;
}


void ls_l(DISK& disk , short back_folder , uchar user = 0){
	int size = 4096;
	for(size_t i=0;i<INDEX_LENGTH;i++)
		for(size_t j=0;j<INDEX_NUM;j++){
			if(disk.d_index[i].fnode[j].id != 0 && disk.d_index[i].fnode[j].back ==back_folder){
				if(disk.d_index[i].fnode[j].id>0){
					size = disk.d_block[(disk.d_index[i].fnode[j].start_block)].file.size;
				}
				cout<<disk.d_index[i].fnode[j].name<<" "<<size<<" priority="<<static_cast<int>(disk.d_index[i].fnode[j].state)<<" user="<<static_cast<int>(disk.d_index[i].fnode[j].userid)<<" ";
				printf(ctime(&disk.d_index[i].fnode[j].time));
				//cout<<endl;
			}  
		}
        cout<<endl;
}




bool test_ls(Command& cmd,DISK& disk,short back_folder , uchar user = 0){
   if(cmd.get_name()=="ls"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_file_no()>0)FLAG_CMD |= 0x41;
	  if(cmd.get_arg_no()==1)FLAG_CMD |= 0x04;
	  if(cmd.get_arg_no()>1)FLAG_CMD |= 0x81;
      if(cmd.find_arg("-l"))FLAG_CMD |= 0x01;
      switch(FLAG_CMD){
          case 0:  ls(disk,back_folder,user);  break;
          case 5: ls_l(disk,back_folder,user);  break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}

/****************************************************************************/
/********************************get_name**********************************/
/****************************************************************************/


string get_name(DISK& disk , string name , short& back_folder, int& state ,uchar user = 0 ){     //返回目录名or文件名
	if(name=="" ||name=="/"){
		state = 0;
		return name;
	}
	else{
		if(name[0]!='/'){
			state = 1;
			return name;          //直接就是名字 不改name 和 back_folder
		}
		else {
			string tmp[10];  int num;
			//short temp = back_folder;
			back_folder = 0;            //从根目录开始
			if(!seperate(name,tmp, num)){       //切割
				cout<<"Wrong path !"<<endl;
				state = 0;
				return name;
			}
			for(size_t k=0;k<num-1;k++){
				if(cd(disk,tmp[k],back_folder ,user,1)){           //如果前级目录已经存在 则进入
					continue;
				}
				else{
					cout<<"directory "<<tmp[k]<<" doesn't exist!"<<endl;
					//back_folder = temp;
					state = 0;
					return name;
				}				
			}
			//back_folder = temp;
			state = 1;
			return tmp[num-1];
		}
	}
}


/****************************************************************************/
/************************************exit************************************/
/****************************************************************************/

//exit
bool test_exit(Command& cmd){
   if(cmd.get_name()== "exit" && cmd.get_arg_no()==0 && cmd.get_file_no()==0)
       return true;
   else return false;
}


/****************************************************************************/
/************************************clear************************************/
/****************************************************************************/
bool test_clear(Command& cmd){
   if(cmd.get_name()!= "clear" || cmd.get_arg_no() || cmd.get_file_no())
       return false;
   else return true;
}

void clear(){
   int i = 30;
   while(i){
     cout<<endl;
     i--;
   }
}

/****************************************************************************/
/************************************mkdir***********************************/
/****************************************************************************/


bool mkdir(DISK& disk, string name , short back_folder , uchar user = 0, char flag = 0){
	size_t i,j;
	bool find =false;
	for(i=0;i<INDEX_LENGTH;i++){
		for(j=0;j<INDEX_NUM;j++){
			if(!disk.d_index[i].fnode[j].id){                    //找到空闲的索引结点 disk.d_index[i].fnode[j] 
				find = true;
				break;
			}
                }
                if(find==true)break;
	}
		if(!find){
			cout<<"Disk is full now!Can not make a directory.\n";
			return false;
		}
       short back = back_folder ;

		if(cd(disk,name,back_folder ,user,1)){
			if(flag==0)cout<<"This dir is already exist : "<<name<<endl;
			//cd_back(disk, back_folder , user);
			return false;
		}

		//找到同名文件。。。。。
	if(back_folder<0){     //不在根目录下
		const char *str = name.c_str();    size_t m,n,t, p,k;
		if(!get_index(back_folder,m,n))     //获得当前目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					cout<<"This file is already exist : "<<name<<endl;
					return false;
				}
			}
		}
	}

	else if(back_folder == 0){   //在根目录下
		//查找根目录下有没有同名文件
		size_t c,d;  const char *str = name.c_str(); 
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件
				if(disk.d_index[c].fnode[d].id>0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){  
					cout<<"File already exist :"<<name<<endl;
					return false;
				}
			}
		}
	}

		back_folder = back; 

	if(name.size()<61){
		const char *str = name.c_str();
		if(str[0]!='/'){
			size_t m,n,t; 
			if(get_index(back_folder, m ,n)){             
				for(t=0;t<INC_FILE;t++){
					if(disk.d_index[m].fnode[n].next[t]==0){
						disk.d_index[m].fnode[n].next[t] = -(i*INDEX_NUM+j+1);
						break;
					}
				}
				if(t==INC_FILE){cout<<"Reach the limit ,can't create!"<<endl;return false;}
			}

			disk.d_index[i].fnode[j].id = -(i*INDEX_NUM+j+1);   //索引盘块号×索引盘块数+（序号+1）  再取正负号
			strcpy(disk.d_index[i].fnode[j].name ,str);
			disk.d_index[i].fnode[j].start_block = 0;
			disk.d_index[i].fnode[j].back = back_folder;
			for(int t=0;t<INC_FILE;t++)
				disk.d_index[i].fnode[j].next[t] = 0;
			disk.d_index[i].fnode[j].state = 0xFF;		
			disk.d_index[i].fnode[j].userid = user;  
			disk.d_index[i].fnode[j].time = time(NULL);
			//增加父节点的目录域next

			return true;
		}
		else {
			//未切分干净
			return false;
		}
	}
	else {cout<<"Too long name,can't make new directory\n";return false;}
}

bool mkdir_2(DISK& disk, string name , short back_folder , uchar user = 0,int flag = 0){    //if flag=0 , 前级文件夹不存在不能创建
	string tmp[10]; int num;  
	if(!seperate(name,tmp, num)){
		cout<<"Incorrect format!"<<endl;
		return false;
	}

	if(flag==0){
		size_t k;
		for(k=0;k<num-1;k++){
			if(cd(disk,tmp[k],back_folder ,user,1)){           //如果前级目录已经存在 则进入
				continue;
			}
			else {                                           //若不存在则报错  flag=0
				cout<<"directory "<<tmp[k]<<" doesn't exist!"<<endl;
				return false;
			}
		}
		if(k==num-1){
			if(!mkdir(disk, tmp[k], back_folder,user))
				return false; 
			else return true;
		}
	}
	else if(flag ==1){
		for(size_t k=0;k<num;k++){
			if(cd(disk,tmp[k],back_folder ,user,1)){           //如果前级目录已经存在 则进入
				continue;
			}
			else{                                                           //若不存在
				if(!mkdir(disk, tmp[k], back_folder,user))
					return false; 
				cd(disk,tmp[k] ,back_folder ,user,1);               //每创建成功就进入那个目录
			}
		}
	}
}

bool test_mkdir(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="mkdir"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数不为0  
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x80;   //参数>1 
		if(cmd.get_file_no()>=1)FLAG_CMD |= 0x02;   //文件夹数为>=1
		if(cmd.find_arg("-p")) FLAG_CMD |= 0x04;      //-p
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x08;  //help
		//cout<<(int )FLAG_CMD<<endl;
      switch(FLAG_CMD){
		  case 2: {
			  for(int i = 0; i<cmd.get_file_no();i++){
				  if(cmd.get_file(i)[0]!='/'){
					  mkdir(disk,cmd.get_file(i),back_folder,user);
				  }
				  else mkdir_2(disk,cmd.get_file(i), 0,user ,0);
			  }
			  break;
		  }
		  case 7:{
			  for(int i = 0; i<cmd.get_file_no();i++){
				  if(cmd.get_file(i)[0]!='/')
					  mkdir(disk,cmd.get_file(i),back_folder,user);
				  else mkdir_2(disk,cmd.get_file(i), 0,user ,1);
			  }
			  break;
		  }
		  case 9:cout<<"****************************\nHELP about mkdir\n  Format: mkdir name1 name2\n\t  mkdir /xxx/xxx\n\t  mkdir -p /xxx/xxx\n****************************"<<endl;break;
          default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}


/****************************************************************************/
/***********************************rmdir************************************/
/****************************************************************************/

//rmdir  删除当前目录下目录
bool rmdir(DISK& disk, string name , short back_folder , uchar user = 0){
	if(name.size()>=61){
		cout<<"Too long name!"<<endl;
		return false;
	}
	const char *str_tmp = name.c_str();
	bool find =false;
	size_t i,j,m,n,t;
	if(back_folder<0){
		if(!get_index(back_folder,i,j))     //获得当前目录所在索引序号
			return false;
		for(t=0;t< INC_FILE ;t++){
			if(disk.d_index[i].fnode[j].next[t]<0){           //查找当前文件夹下有没有要删除的目录
				m = (-disk.d_index[i].fnode[j].next[t]-1)/32;
				n = (-disk.d_index[i].fnode[j].next[t]-1)%32; 
				if(strcmp(disk.d_index[m].fnode[n].name , str_tmp)==0){      //找到同名的
					for(size_t g=0;g< INC_FILE ;g++){
						if(disk.d_index[m].fnode[n].next[g]!=0){                  //目录非空
							cout<<"Directory "<<name<<" is not empty!"<<endl;
							return false;
						}
					}
					find = true;
					break;
				}
			}
		}
	}
	else if(back_folder==0){
		for(m=0;m<INDEX_LENGTH;m++){
			for(n=0;n<INDEX_NUM;n++){
				if(disk.d_index[m].fnode[n].id<0 && disk.d_index[m].fnode[n].back == 0 && strcmp(disk.d_index[m].fnode[n].name , str_tmp)==0){              
					for(size_t g=0;g< INC_FILE ;g++){
						if(disk.d_index[m].fnode[n].next[g]!=0){                  //目录非空
							cout<<"Directory "<<name<<" is not empty!"<<endl;
							return false;
						}
					}
					find = true;
					break;
				}
			}
			if(find == true)
				break;
		}
	}
	if(find){
		disk.d_index[m].fnode[n].id = 0;      //删除该目录
		if(back_folder!=0)disk.d_index[i].fnode[j].next[t] = 0;  //删除该目录的父目录中的链接值
		return true;
	}
	else {
		if(name!="")cout<<"Dir "<<name<<" doesn't exist!"<<endl;
		return false;
	}
}

/****************************************************************************/
/*************************************rm************************************/
/****************************************************************************/

bool rm(DISK& disk, string name , short back_folder , uchar user = 0, char flag = 0){        //flag=0 不能删除文件夹 
	//获得文件or文件夹名
	int state;  bool dir ;
	string str_name = get_name(disk, name , back_folder,state,user); 
	if(state == 0)return false;
	//str_name为最后的文件/文件夹名字   back_folder为该文件or文件夹的上级目录
	
	//看是否存在该文件or文件夹  获得上级目录号  并且得到该节点
	const char *str = str_name.c_str(); 
	bool find = false;
	size_t c,d;     //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0

	if(back_folder == 0){
		
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹
				if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){  
					find = true;
					if(disk.d_index[c].fnode[d].id>0)dir = false;
					else dir = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
	}
	else if(back_folder<0){
		if(!get_index(back_folder,m,n))     //获得目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = false;
					break;
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]<0){                //存在文件夹
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = true;
					break;
				}
			}
		}
	}

	if(!find){
		cout<<"File "<<str_name<<" doesn't exist!"<<endl;
		return false;
	}

	if(flag==2){
		if(!dir){                     //要删除的不是文件夹  
			cout<<"Not a directory!"<<endl;
			return false;
		}
	}

//存在  可以删除文件夹
	else if(flag == 1){
		if(!dir){       //不是文件夹
			rm(disk, str_name , back_folder , user, 0);   //用goto也行
			return true;
		}


		else{       //是文件夹
			size_t u,v;
			if(back_folder==0){        //是根目录
				for(size_t l=0;l<INC_FILE;l++){
					if(disk.d_index[c].fnode[d].next[l]>0){
						get_index_2(disk.d_index[c].fnode[d].next[l], u , v);
						rm(disk, disk.d_index[u].fnode[v].name, disk.d_index[c].fnode[d].id , user, 0); 
					}		
					else if(disk.d_index[c].fnode[d].next[l]<0){
						get_index(disk.d_index[c].fnode[d].next[l], u , v);
						rm(disk, disk.d_index[u].fnode[v].name, disk.d_index[c].fnode[d].id , user, 1); 
					}
				}
				//删除自己
				rmdir(disk,  disk.d_index[c].fnode[d].name,  back_folder, user);
			}
			else if(back_folder<0){
				for(size_t l=0;l<INC_FILE;l++){
					if(disk.d_index[p].fnode[k].next[l]>0){
						get_index_2(disk.d_index[p].fnode[k].next[l], u , v);
						rm(disk, disk.d_index[u].fnode[v].name, disk.d_index[p].fnode[k].id , user, 0);       //递归调用
					}		
					else if(disk.d_index[p].fnode[k].next[l]<0){
						get_index(disk.d_index[p].fnode[k].next[l], u , v);
						rm(disk, disk.d_index[u].fnode[v].name, disk.d_index[p].fnode[k].id , user, 1); 
					}		
				}
				//删除自己
				rmdir(disk,  disk.d_index[p].fnode[k].name,  back_folder, user);
			}
			return true;
		}
	}

//存在 不可以删除文件夹

	else if(flag==0){
    //判断是否为文件  是文件则继续
		if(dir){
			cout<<str_name<<" is not a file , can not delete !"<<endl;
			return false;
		}

	//删除该文件信息
	if(back_folder==0){
			disk.d_index[c].fnode[d].id = 0;             //释放FNODE  不需要释放block
			disk.d_index[c].fnode[d].back = 0;

			int start = disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.start;
			int end = start + disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.total ;
			for(int s=start ;s<end; s++)          //释放table
				disk.d_table.state[s] = 0;          
		}
		else if(back_folder<0){            
			disk.d_index[p].fnode[k].id = 0;
			disk.d_index[p].fnode[k].back = 0;
			disk.d_index[m].fnode[n].next[t] = 0;     //去除上级关联
			int start = disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.start;
			int end = start + disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.total ;
			for(int s=start ;s<end; s++)          //释放table
				disk.d_table.state[s] = 0; 
		}

	}
	return true;

}




bool test_rm(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="rm"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数不为0  
		if(cmd.get_file_no()>=1)FLAG_CMD |= 0x02;   //文件夹数为>=1
		if(cmd.find_arg("-r")) FLAG_CMD |= 0x04;        //-r
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x08;  //help
		switch(FLAG_CMD){
		  case 2: {
			  for(int i = 0; i<cmd.get_file_no();i++)
				  rm(disk,cmd.get_file(i),back_folder,user,0);
			  break;
		  }
		  case 7:{
			  for(int i = 0; i<cmd.get_file_no();i++)
				  rm(disk,cmd.get_file(i),back_folder,user,1);
			  break;
		  }  
		  case 9:cout<<"****************************\nHELP about rm\n  Format: rmdir name1 name2\n\t  rm /xxx/xxx\n\t  rmdir -r /xxx/xxx\n****************************"<<endl;break;
          default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}



/****************************************************************************/
/**********************************rmdir2***********************************/
/****************************************************************************/




bool rmdir_2(DISK& disk, string name , short back_folder , uchar user = 0,int flag = 0){    //if flag=0 , 前级文件夹不存在不能创建
	string tmp[10]; int num;  
	if(!seperate(name,tmp, num)){
		cout<<"Incorrect format!"<<endl;
		return false;
	}
	size_t k;
	for(k=0;k<num-1;k++){
		//cout<<tmp[k]<<" ";
		if(cd(disk,tmp[k],back_folder ,user,1)){           //如果目录已经存在 则进入
			//cout<<k<<" "; 
			continue;
		}
		else {                                             //若不存在则报错  
			cout<<"directory "<<tmp[k]<<" doesn't exist!"<<endl;
			return false;
		}
	}

	if(flag==0){            //flag =0 只删除最后一个目录  对应没有-p的情形
		if(rmdir(disk,tmp[k], back_folder , user))
			return true;
		else return false;
	}
	else if(flag ==1){     //flag =1 删除所有目录  对应有-p的情形
		while(k>=0){
			if(rmdir(disk,tmp[k], back_folder , user)){
				cd_back(disk, back_folder , user);
				k--;
			}
			else return false;
		}
		return true;
	}
}

bool test_rmdir(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="rmdir"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数不为0  
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x80;      //参数>1 
		if(cmd.get_file_no()>=1)FLAG_CMD |= 0x02;   //文件夹数为>=1
		if(cmd.find_arg("-p")) FLAG_CMD |= 0x04;      //-p
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x08;  //help
		switch(FLAG_CMD){
		  case 2: {
			  for(int i = 0; i<cmd.get_file_no();i++){
				  if(cmd.get_file(i)[0]!='/'){
					  rmdir(disk,cmd.get_file(i),back_folder,user);
				  }
				  else rmdir_2(disk,cmd.get_file(i), 0,user ,0);
			  }
			  break;
		  }
		  case 7:{
			  for(int i = 0; i<cmd.get_file_no();i++){
				  rm(disk,cmd.get_file(i), back_folder ,user,2);
			  }
			  break;
		  }  
		  case 9:cout<<"****************************\nHELP about rmdir\n  Format: rmdir name1 name2\n\t  rmdir /xxx/xxx\n\t  rmdir -p /xxx/xxx\n****************************"<<endl;break;
          default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}


/****************************************************************************/
/***********************************touch**********************************/
/****************************************************************************/

bool touch(DISK& disk, string name ,short back_folder , uchar user = 0,char flag = 0){
	size_t i,j;                          // i  j 对应找到的空闲结点
	bool find =false, find2 = false;
	for(i=0;i<INDEX_LENGTH;i++){
		for(j=0;j<INDEX_NUM;j++){
			if(!disk.d_index[i].fnode[j].id){                     //找到空闲的索引结点 disk.d_index[i].fnode[j] 
				find = true;
				break;
			}
                }
                if(find==true)break;
	}

	//找到空闲的磁盘块   找不到 find2 = false
	size_t z;
	for(z=0;z<TABLE_LENGTH;z++){ 
		if(disk.d_table.state[z]==0){                            //找到磁盘块为z
			find2 = true;
			break;
		}
	}

	if(!find || !find2){
		cout<<"Disk is full now!Can not make a file.\n";
		return false;
	}

	const char *str = name.c_str();
	size_t m,n,t, p,k;

	if(back_folder<0){     //不在根目录下
		if(!get_index(back_folder,m,n))     //获得当前目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件夹没   存在则修改时间
			if(disk.d_index[m].fnode[n].next[t]<0){    
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					//cout<<"This file is already exist : "<<name<<endl;
					disk.d_index[p].fnode[k].time = time(NULL);
					return true;
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					//cout<<"This file is already exist : "<<name<<"."<<type<<endl;
					disk.d_index[p].fnode[k].time = time(NULL);
					return true;
				}
			}
		}


			if(flag==0){
				cout<<"can't find the file :"<<name<<endl;
				return true;
			}

			//不存在同名文件 则新建
			if(name.size()<61){               //检查文件名和文件类型合法性
			//在上级目录下看满了没 如果没满就添加 满了报错 退出

				if(get_index(back_folder, p ,k)){

					for(t=0;t<INC_FILE;t++){
						if(disk.d_index[p].fnode[k].next[t]==0){
							disk.d_index[p].fnode[k].next[t] =  (i*INDEX_NUM+j+1);        //增加父节点的目录域next
							break;
						}
					}

					if(t==INC_FILE){cout<<"Reach the limit ,can't create!"<<endl;return false;}
				}

				//添加FNODE
				disk.d_index[i].fnode[j].id = (i*INDEX_NUM+j+1);   //索引盘块号×索引盘块数+（序号+1）  再取正号
				strcpy(disk.d_index[i].fnode[j].name ,str);
				disk.d_index[i].fnode[j].start_block = z;                   //起始盘号
				disk.d_index[i].fnode[j].back = back_folder;
				for(int t=0;t<INC_FILE;t++)
					disk.d_index[i].fnode[j].next[t] = 0;                      //此句无用
				disk.d_index[i].fnode[j].state = 0xFF;		
				disk.d_index[i].fnode[j].userid = user;  
				disk.d_index[i].fnode[j].time = time(NULL);
				
				//添加对应的磁盘块

				disk.d_block[z].file.size =32;                       //文件大小 空文件就是32B 
				disk.d_block[z].file.start = z ;                      //起始盘块
				disk.d_block[z].file.total = 1;                       //共占有盘块数 
				strcpy(disk.d_block[z].file.Data,"");
				
				//在table中记录该文件
				disk.d_table.state[z] = 1;
			}
			else {
				cout<<"Illegal name or type"<<endl;
				return false;
			}
		}

	
	

	else if(back_folder == 0){   //在根目录下
		if(name.size()<61){               //检查文件名和文件类型合法性
			//查找根目录下有没有同名文件
			size_t c,d;
			for(c=0;c<INDEX_LENGTH;c++){
				for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹     则修改时间
					if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){                    
						//cout<<"File already exist :"<<name<<endl;
						disk.d_index[c].fnode[d].time = time(NULL);
						return true;
					}
				}
			}
			//找到同名文件报错 没有则创建文件

			if(flag==0){
				cout<<"can't find the file :"<<name<<endl;
				return true;
			}

			//添加FNODE				
			disk.d_index[i].fnode[j].id = (i*INDEX_NUM+j+1);   //索引盘块号×索引盘块数+（序号+1）  再取正号
			strcpy(disk.d_index[i].fnode[j].name ,str);
			disk.d_index[i].fnode[j].start_block = z;                   //起始盘号
			disk.d_index[i].fnode[j].back = 0;
			for(int t=0;t<INC_FILE;t++)
				disk.d_index[i].fnode[j].next[t] = 0;                      //此句无用
			disk.d_index[i].fnode[j].state = 0xFF;		
			disk.d_index[i].fnode[j].userid = user;  
			disk.d_index[i].fnode[j].time = time(NULL);
			
			//添加对应的磁盘块 
			disk.d_block[z].file.size =32;                       //文件大小 空文件就是32B 
			disk.d_block[z].file.start = z ;                      //起始盘块
			disk.d_block[z].file.total = 1;                       //共占有盘块数 
			strcpy(disk.d_block[z].file.Data,"");
			
			//在table中记录该文件
			disk.d_table.state[z] = 1;

		}
		else {
			cout<<"Illegal name or type"<<endl;
			return false;
		}
	}
	return true;

}




bool test_touch(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="touch"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数为1 
		if(cmd.get_file_no()>0)FLAG_CMD |= 0x02;     //名字>0
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x80;
		if(cmd.find_arg("-c"))FLAG_CMD |= 0x08;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x04; 

		int state ;
		string str;   short temp = back_folder;
		switch(FLAG_CMD){
		  case 2: 	     //需要写一个区分名字和类型的函数
			  {
				  for(size_t n=0;n<cmd.get_file_no();n++){                                      
					  str = get_name(disk, cmd.get_file(n) , back_folder,state,user); 
					  if(state == 1)
						  touch(disk,str , back_folder ,user,1);	
					  back_folder = temp;
				  }
				  break;
			  }
		  case 5:	cout<<"****************************\nHELP about touch\n  Format: touch name1 name2\n\t  touch /xxx/xxx\n\t  rmdir -c /xxx/xxx\n****************************"<<endl;	  break;
		  case 11:	
			  {
				  for(size_t n=0;n<cmd.get_file_no();n++){
					  str = get_name(disk, cmd.get_file(n) , back_folder, state,user);
					  if(state==1)
						  touch(disk,str ,back_folder ,user,0);	
					  back_folder = temp;
				  }
				  break;
			  }
          default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}




/****************************************************************************/
/**************************************vi************************************/
/****************************************************************************/

bool vi(DISK& disk, string name ,short back_folder , uchar user = 0){       
	/*
	如果文件已经存在 那么只能4KB大小最多   
	如果输入name为空  那么可以8KB最多
	*/
	int state;
	string str_name ;

	uchar line = 0;      //计算文件总行数  最大 255 
	char flag= 1;     //0 丢弃  1保存 
	string str[BLOCK_SIZE/LINE_LENGTH-1];                     //开辟第一个块的空间
	string str2[BLOCK_SIZE/LINE_LENGTH];                      //第二个块的空间备用
	uchar total = 1;                       //总共几个块
	size_t i =0 ,j= 0;
	for(i=0;i<BLOCK_SIZE/LINE_LENGTH-1; i++){              //输入第一个块的数据
		getline(cin,str[i]);
		while((str[i].size()>31)){
			cout<<"This line is too long ,please retype :";
			getline(cin,str[i]);
		}
		if(str[i]==":q" || str[i]==":Q"){flag = 0;break;}
		else if(str[i]==":S" || str[i]==":s"){flag = 1;   break;  }        //保存
	}

	if(i==(BLOCK_SIZE/LINE_LENGTH-1)){        //一块不够  再开一块   
		string temp;
		total = 2;
		cout<<"Do you want to continue(Y/N) :";
		do{
			cin>>temp;
			if(temp=="Y" || temp=="y"){
				total = 2;
				for(j=0;j<BLOCK_SIZE/LINE_LENGTH; j++){
					getline(cin,str2[j]);
					while((str2[j].size()>31)){
						cout<<"This line is too long ,please retype :";
						getline(cin,str2[j]);
					}
					if(str2[j]==":q" || str2[j]==":Q"){flag = 0;break;}
					else if(str2[j]==":S" || str2[j]==":s"){flag = 1;    break;}         //保存
				}
				if(j==BLOCK_SIZE/LINE_LENGTH){
					cout<<"Reach the max length !"<<endl;
					flag = 1;
				}
				break;
			}          
			else if(temp=="N" || temp=="n");
			else cout<<"Please type in Y or N :"<<endl;
		}while(temp!="N" && temp!="n");
	}
	
	if(name!=""){
		str_name = get_name(disk, name , back_folder,state,user);          //若文件名不为空   先获得文件名和上级目录
		if(state == 0)return false;
	}
	else {
		//询问保存信息
		string tmp;
		cout<<"Save it (Y/N)? Type in :";
		do{
			cin>>tmp;
			if(tmp=="Y" || tmp=="y"){
				cout<<"Please type in the name ,with or without the whole path :";
				cin>>name;
				while(name==""){
					cout<<"Empty name ! Retype in it :";
					cin>>name;
				}
				flag = 1;
				break;
			}          
			else if(tmp=="N" || tmp=="n");
			else cout<<"Please type in Y or N :"<<endl;
		}while(tmp!="N" && tmp!="n");

		str_name = get_name(disk, name , back_folder,state,user);          //先获得文件名
		if(state == 0)return false;
	}

	line = i+j;         //line为总行数   size = 32 + line * 32     line =( size - 32 ) / 32

	if(flag == 1){
		touch(disk, str_name ,back_folder ,user,1);      //不存在则创建  存在则修改时间
		const char *STR = str_name.c_str(); 
		bool find = false;
		size_t c,d;     //back_folder = 0
		size_t m,n,t, p,k;    //back_folder<0
		ushort start ;
		if(back_folder == 0){		
			for(c=0;c<INDEX_LENGTH;c++){
				for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹
					if(disk.d_index[c].fnode[d].id>0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , STR)==0  ){  
						find = true;
						break;
					}
				}
				if(find == true)break;                 //找到同名文件or文件夹
			}
			start = disk.d_index[c].fnode[d].start_block;
		}
		else if(back_folder<0){
			get_index(back_folder,m,n);     //获得目录所在索引序号  m  n
			for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
				if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
					p = (disk.d_index[m].fnode[n].next[t]-1)/32;
					k = (disk.d_index[m].fnode[n].next[t]-1)%32;
					if(strcmp(disk.d_index[p].fnode[k].name , STR)==0){      //找到同名的
						find = true;
						break;
					}
				}
			}
			start = disk.d_index[p].fnode[k].start_block ;
		}

		size_t h=0,q=0;
		while(h<i){
			const char *str_1 = str[h].c_str();
			strncpy(&disk.d_block[start].file.Data[h*LINE_LENGTH] , str_1,LINE_LENGTH);
			h++;  
		}
		
		if(total == 2){    //需要加块
			if(disk.d_table.state[(start+1)]>0 || disk.d_block[start].file.total==1 ){    //下一块满了
				cout<<"Can't save the file ,becasue the file in out of max size!"<<endl;
				return false;
			}
			else {
				disk.d_table.state[(start+1)]=2;
				disk.d_block[start].file.total=2;
			}
		
			while(q<j){
				const char *str_2 = str2[q].c_str();
				strncpy(&disk.d_block[start].file.Data[q*LINE_LENGTH] , str_2, LINE_LENGTH);
				q++;
			}
		}

		disk.d_block[start].file.size=line*LINE_LENGTH;            //更改文件大小
	}

	return true;
}



bool test_vi(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="vi"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x80;     //参数大于0 
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数等于1
		if(cmd.get_file_no()==1)FLAG_CMD |= 0x02;      //名字>0
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x04;            //5
		if(cmd.get_file_no()==0)FLAG_CMD |= 0x08;      //名字>0

		switch(FLAG_CMD){
		  case 2:	vi(disk,cmd.get_file(0) ,back_folder ,user);  break;
		  case 5:	cout<<"****************************\nHELP about vi\n  Format: vi name\n****************************"<<endl;	 break;
          case 8:	vi(disk,"" ,back_folder ,user);	  break;
		  default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}





/****************************************************************************/
/**************************************cat***********************************/
/****************************************************************************/

bool cat(DISK& disk, string name ,short back_folder , uchar user = 0, char flag = 0){        //FLAG = 0 原样显示  FLAG = 1 连续两行及以上空行只显示一行       
	/*
	检查文件是否存在  存在则继续

	打开文件并找到

	输出文件内容
	*/

	int state;   	
	string str_name ;
	str_name = get_name(disk, name , back_folder,state,user);          //先获得文件名和上级目录
	if(state == 0)return false;

	const char *STR = str_name.c_str(); 
	bool find = false;
	size_t c,d;              //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0
	ushort start ;             //标记该文件起始块
	uchar total , line ;              //保存文件块数和行数   line = (size - 32)/LINE_LENGTH
	string str[BLOCK_SIZE/LINE_LENGTH-1];                     //开辟第一个块的空间
	string str2[BLOCK_SIZE/LINE_LENGTH];                      //第二个块的空间备用

	if(back_folder == 0){		
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件
				if(disk.d_index[c].fnode[d].id>0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , STR)==0  ){  
					find = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
		start = disk.d_index[c].fnode[d].start_block;
	}
	else if(back_folder<0){
		get_index(back_folder,m,n);     //获得目录所在索引序号  m  n
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , STR)==0){      //找到同名的
					find = true;
					break;
				}
			}
		}
		start = disk.d_index[p].fnode[k].start_block ;
	}                                       

	if(!find){
		cout<<"Can't find the file "<<str_name<<" ,it doesn't exist!"<<endl;
		return false;
	}
	//至此已经找到了该文件

	total = disk.d_block[start].file.total ;
	line = (disk.d_block[start].file.size - 32)/32;
	char tempstr[32];

	int num = 0;

	if(total == 1){
		size_t h=0;
		while(h<=line){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				if(flag==1){
					if(str[h]==""){
						num++;
						if(num>1);
						else cout<<str[h]<<endl;
					}
					else {
						num = 0;
						cout<<str[h]<<endl;
					}
				}
				else cout<<str[h]<<endl;
			}
			h++;  
		}
		
	}
	else if(total==2){
		size_t h=0, q=0;
		while(h<127){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				if(flag==1){
					if(str[h]==""){
						num++;
						if(num>1);
						else cout<<str[h]<<endl;
					}
					else {
						num = 0;
						cout<<str[h]<<endl;
					}
				}
				else cout<<str[h]<<endl;
			}
			h++;  
		}
		while(q<=line-127){
			strncpy(tempstr, &disk.d_block[start].file.Data[q*LINE_LENGTH], LINE_LENGTH );
			str2[q] = tempstr ;
			if(str2[q]!=":s" && str2[q]!=":S"){
				if(flag==1){
					if(str2[q]==""){
						num++;
						if(num>1);
						else cout<<str2[q]<<endl;
					}
					else {
						num = 0;
						cout<<str2[q]<<endl;
					}
				}
				else cout<<str2[q]<<endl;
			}
			q++;
		}
	}
	return true;
}

/*
cat xxx         0010
cat -s xxx     0111
cat --help     1001
*/

bool test_cat(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="cat"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数为1 
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x80;     //参数大于1 
		if(cmd.get_file_no()==1)FLAG_CMD |= 0x02;    //名字==1
		if(cmd.find_arg("-s"))FLAG_CMD |= 0x04;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x08;

		switch(FLAG_CMD){
		  case 2:	cat(disk,cmd.get_file(0) ,back_folder ,user,0);  break;
		  case 9:	cout<<"****************************\nHELP about cat\n  Format: cat name\n\t cat -s name\n****************************"<<endl;	 break;
          case 7:	cat(disk,cmd.get_file(0) ,back_folder ,user, 1);	  break;
		  default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}


/****************************************************************************/
/**************************************wc************************************/
/****************************************************************************/
/*
wc
显示文件字节数-c  字符数-m   行数-l
*/

bool wc(DISK& disk, string name ,short back_folder , char C, char M ,char L, uchar user = 0){        //几个char  0不显示  1显示
	int state;   	
	string str_name ;
	str_name = get_name(disk, name , back_folder,state,user);          //先获得文件名和上级目录
	if(state == 0)return false;

	const char *STR = str_name.c_str(); 
	bool find = false;
	size_t c,d;              //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0
	ushort start ;             //标记该文件起始块
	uchar total , line ;              //保存文件块数和行数   line = (size - 32)/LINE_LENGTH
	string str[BLOCK_SIZE/LINE_LENGTH-1];                     //开辟第一个块的空间
	string str2[BLOCK_SIZE/LINE_LENGTH];                      //第二个块的空间备用

	if(back_folder == 0){		
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件
				if(disk.d_index[c].fnode[d].id>0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , STR)==0  ){  
					find = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
		start = disk.d_index[c].fnode[d].start_block;
	}
	else if(back_folder<0){
		get_index(back_folder,m,n);     //获得目录所在索引序号  m  n
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , STR)==0){      //找到同名的
					find = true;
					break;
				}
			}
		}
		start = disk.d_index[p].fnode[k].start_block ;
	}                                       

	if(!find){
		cout<<"Can't find the file "<<str_name<<" ,it doesn't exist!"<<endl;
		return false;
	}
	//至此已经找到了该文件

	total = disk.d_block[start].file.total ;
	line = (disk.d_block[start].file.size - 32)/32;
	char tempstr[32];

	/*
	文件字节数为文件大小  即  disk.d_block[start].file.size
	字符数需要计算     numOFchar
	行数已经在line中 
	*/
	int numOFchar = 0;

	if(total == 1){
		size_t h=0;
		while(h<=line){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				numOFchar += str[h].size();
			}
			h++;  
		}
	}
	else if(total==2){
		size_t h=0, q=0;
		while(h<127){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				numOFchar += str[h].size();
			}
			h++;  
		}
		while(q<=line-127){
			strncpy(tempstr, &disk.d_block[start].file.Data[q*LINE_LENGTH], LINE_LENGTH );
			str2[q] = tempstr ;
			if(str2[q]!=":s" && str2[q]!=":S"){
				numOFchar += str2[q].size();
			}
			q++;
		}
	}

	if(C==1)
		cout<<disk.d_block[start].file.size<<"  ";
	if(M==1)
		cout<<numOFchar<<"  ";
	if(L==1)
		cout<<(int)(line+1)<<"  ";
	cout<<name<<endl;

	return true;
}


bool test_wc(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="wc"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数为1 
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x02;     
		if(cmd.get_file_no()>=1)FLAG_CMD |= 0x04;    //名字==1
		if(cmd.find_arg("-l"))FLAG_CMD |= 0x08;
		if(cmd.find_arg("-m"))FLAG_CMD |= 0x10;
		if(cmd.find_arg("-c"))FLAG_CMD |= 0x20;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x40;

		switch(FLAG_CMD){
		  case 4:	 for(int i = 0; i<cmd.get_file_no();i++)wc(disk,cmd.get_file(i) ,back_folder,1,1,1 ,user);  break;
		  case 37:	 for(int i = 0; i<cmd.get_file_no();i++)wc(disk,cmd.get_file(i) ,back_folder,1,0,0 ,user);  break;
		  case 21:	 for(int i = 0; i<cmd.get_file_no();i++)wc(disk,cmd.get_file(i) ,back_folder,0,1,0 ,user);  break;
		  case 7:	 for(int i = 0; i<cmd.get_file_no();i++)wc(disk,cmd.get_file(i) ,back_folder,0,0,1 ,user);  break;
		  case 65:	cout<<"****************************\nHELP about wc\n  Format: wc name1,name2\n\twc -c name\n\twc -m name\n\twc -l name\n\t****************************"<<endl;	 break;
		  default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}



/****************************************************************************/
/**************************************df************************************/
/****************************************************************************/
/*
df                                             --help     -k   -i     file>0  arg>1  arg=1
df     显示磁盘块的使用情况   00000000         0
df -i  显示索引块使用状况      00001001         9
df -k 以kb为单位显示            00010001         17
df --help                              00100001         33
显示内容   
*/


bool df(DISK& disk,short back_folder, uchar user = 0, char inode = 0, char kb = 0){
	if(inode == 1){          //查看索引块
		int inode_used = 0;
		for(size_t i=0;i<INDEX_LENGTH;i++)
			for(size_t j=0;j<INDEX_NUM;j++){
				if(disk.d_index[i].fnode[j].id != 0){
					inode_used++;
				}
			}
			cout<<"Information of inode used :"<<endl;
			cout<<"Total :"<<INDEX_LENGTH*INDEX_NUM<<endl;
			cout<<"Used :"<<inode_used<<endl;
			cout<<"Free : "<<(INDEX_LENGTH*INDEX_NUM-inode_used)<<endl;
	}
	else {                        //查看磁盘块
		int disk_used = (INDEX_LENGTH+1)*BLOCK_SIZE;
		for(size_t i=0;i<(BLOCK_LENGTH-INDEX_LENGTH-1);i++){
			if(disk.d_table.state[i] == 1){
				disk_used += disk.d_block[i].file.size;
			}
		}
		if(kb==1){
			cout<<"Information of disk used (Print by KB) :"<<endl;
			cout<<"Total :"<<BLOCK_SIZE*BLOCK_LENGTH/1024<<endl;
			cout<<"Used :"<<disk_used/1024<<endl;
			cout<<"Free : "<<(BLOCK_SIZE*BLOCK_LENGTH-disk_used)/1024<<endl;
		}
		else {
			cout<<"Information of disk used (Print by Byte):"<<endl;
			cout<<"Total :"<<BLOCK_SIZE*BLOCK_LENGTH<<endl;
			cout<<"Used :"<<disk_used<<endl;
			cout<<"Free : "<<(BLOCK_SIZE*BLOCK_LENGTH-disk_used)<<endl;
		}
	}
	return true;
}

bool test_df(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="df"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数等于1
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x02;     //参数大于1
		if(cmd.get_file_no()>0)FLAG_CMD |= 0x04;      //名字>0
		if(cmd.find_arg("-i"))FLAG_CMD |= 0x08;         
		if(cmd.find_arg("-k"))FLAG_CMD |= 0x10;     
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x20;            

		switch(FLAG_CMD){
		  case 0:	df(disk,back_folder ,user);  break;
		  case 9:	df(disk,back_folder ,user,1,0);  break;
		  case 17:	df(disk,back_folder ,user,0,1);  break;
		  case 33:	cout<<"****************************\nHELP about df\n  Format: df\n\t df -i\n\t df -k\n****************************"<<endl;	 break;
		  default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}


/****************************************************************************/
/**************************************du************************************/
/****************************************************************************/


bool du(DISK& disk, string name , short back_folder, int &SIZE_TOTAL , uchar user = 0, char flag = 0){        //flag=1对应 -s  只计算总和
	//获得文件or文件夹名
	int state;  bool dir ;
	string str_name = get_name(disk, name , back_folder,state,user); 
	if(state == 0)return false;
	//str_name为最后的文件/文件夹名字   back_folder为该文件or文件夹的上级目录
	
	//看是否存在该文件or文件夹  获得上级目录号  并且得到该节点
	const char *str = str_name.c_str(); 
	bool find = false;
	size_t c,d;     //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0

	int size_total[INC_FILE] = {0};

	if(back_folder == 0){	
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹
				if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){  
					find = true;
					if(disk.d_index[c].fnode[d].id>0)dir = false;
					else dir = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
	}
	else if(back_folder<0){
		if(!get_index(back_folder,m,n))     //获得目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = false;
					break;
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]<0){                //存在文件夹
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = true;
					break;
				}
			}
		}
	}

	if(!find){
		cout<<"File "<<str_name<<" doesn't exist!"<<endl;
		return false;
	}

//存在  
	 if(flag == 1){                             //只计算总和
		if(dir){     //文件夹
			SIZE_TOTAL = 0;
			size_t u,v;
			if(back_folder==0){
				//显示文件大小
				for(size_t x=0;x<INC_FILE;x++){
					if(disk.d_index[c].fnode[d].next[x]>0){
						get_index_2(disk.d_index[c].fnode[d].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[c].fnode[d].id  , size_total[x] , user, 2);
					}
					else if(disk.d_index[c].fnode[d].next[x]<0){
						get_index(disk.d_index[c].fnode[d].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[c].fnode[d].id  , size_total[x] , user, 2);
					}
					SIZE_TOTAL += size_total[x];
				}
				cout<<"Dir："<<disk.d_index[c].fnode[d].name<<" "<<SIZE_TOTAL<< endl;
				return true;
			}
			else if(back_folder<0){    
				for(size_t x=0;x<INC_FILE;x++){
					if(disk.d_index[p].fnode[k].next[x]>0){
						get_index_2(disk.d_index[p].fnode[k].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[p].fnode[k].id  , size_total[x] , user, 2);
					}
					else if(disk.d_index[p].fnode[k].next[x]<0){
						get_index(disk.d_index[p].fnode[k].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[p].fnode[k].id  , size_total[x] , user, 2);
					}
					SIZE_TOTAL += size_total[x];
				}
				cout<<"Dir："<<disk.d_index[p].fnode[k].name<<" "<<SIZE_TOTAL<< endl;
				return true;
			}
		}

		else{
			//显示该文件信息
			if(back_folder==0){
				//显示文件大小
				cout<<"File : "<<disk.d_index[c].fnode[d].name<<" "<<disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.size<<endl;
				SIZE_TOTAL = disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.size;         //返回用
				return true;
			}
			else if(back_folder<0){            
				cout<<"File : "<<disk.d_index[p].fnode[k].name<<" "<<disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.size<<endl;
				SIZE_TOTAL = disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.size;
				return true;
			}
		}
		return true;
	}

//存在 显示全部文件大小

	 else if(flag==2){
		if(dir){     //文件夹
			SIZE_TOTAL = 0;
			size_t u,v;
			if(back_folder==0){
				//显示文件大小
				for(size_t x=0;x<INC_FILE;x++){
					if(disk.d_index[c].fnode[d].next[x]>0){
						get_index_2(disk.d_index[c].fnode[d].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[c].fnode[d].id  , size_total[x] , user, 2);
					}
					else if(disk.d_index[c].fnode[d].next[x]<0){
						get_index(disk.d_index[c].fnode[d].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[c].fnode[d].id  , size_total[x] , user, 2);
					}
					SIZE_TOTAL += size_total[x];
				}
				return true;
			}
			else if(back_folder<0){    
				for(size_t x=0;x<INC_FILE;x++){
					if(disk.d_index[p].fnode[k].next[x]>0){
						get_index_2(disk.d_index[p].fnode[k].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[p].fnode[k].id  , size_total[x] , user, 2);
					}
					else if(disk.d_index[p].fnode[k].next[x]<0){
						get_index(disk.d_index[p].fnode[k].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[p].fnode[k].id  , size_total[x] , user, 2);
					}
					SIZE_TOTAL += size_total[x];
				}
				return true;
			}
		}

		else{
			//显示该文件信息
			if(back_folder==0){
				SIZE_TOTAL = disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.size;         //返回用
				return true;
			}
			else if(back_folder<0){            
				SIZE_TOTAL = disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.size;
				return true;
			}
		}
		return true;
	 }

	else if(flag==0){
		if(dir){     //文件夹
			SIZE_TOTAL = 0;
			size_t u,v;
			if(back_folder==0){
				//显示文件大小
				for(size_t x=0;x<INC_FILE;x++){
					if(disk.d_index[c].fnode[d].next[x]>0){
						get_index_2(disk.d_index[c].fnode[d].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[c].fnode[d].id  , size_total[x] , user, 0);
					}
					else if(disk.d_index[c].fnode[d].next[x]<0){
						get_index(disk.d_index[c].fnode[d].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[c].fnode[d].id  , size_total[x] , user, 0);
					}
					SIZE_TOTAL += size_total[x];
				}
				cout<<"Dir :"<<disk.d_index[c].fnode[d].name<<" "<<SIZE_TOTAL<< endl;
				return true;
			}
			else if(back_folder<0){    
				for(size_t x=0;x<INC_FILE;x++){
					if(disk.d_index[p].fnode[k].next[x]>0){
						get_index_2(disk.d_index[p].fnode[k].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[p].fnode[k].id  , size_total[x] , user, 0);
					}
					else if(disk.d_index[p].fnode[k].next[x]<0){
						get_index(disk.d_index[p].fnode[k].next[x], u , v);
						du(disk,  disk.d_index[u].fnode[v].name,   disk.d_index[p].fnode[k].id  , size_total[x] , user, 0);
					}
					SIZE_TOTAL += size_total[x];
				}
				cout<<"Dir :"<<disk.d_index[p].fnode[k].name<<" "<<SIZE_TOTAL<< endl;
				return true;
			}
		}

		else{
			//显示该文件信息
			if(back_folder==0){
				//显示文件大小
				cout<<"File : "<<disk.d_index[c].fnode[d].name<<" "<<disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.size<<endl;
				SIZE_TOTAL = disk.d_block[(disk.d_index[c].fnode[d].start_block)].file.size;         //返回用
				return true;
			}
			else if(back_folder<0){            
				cout<<"File : "<<disk.d_index[p].fnode[k].name<<" "<<disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.size<<endl;
				SIZE_TOTAL = disk.d_block[(disk.d_index[p].fnode[k].start_block)].file.size;
				return true;
			}
		}
		return true;
	}
}

/*
du  显示当前目录下所有文件大小
du 文件名 文件夹名  显示文件or文件夹名大小
du -s 只计算总和(文件照常计算  文件夹就不计算了)
 
                    --help  -s  name>=1  arg>1  arg=1
du               00000000      0  当前目录
du xxx         00000100      4
du -s xxx     00001101      13
du -s           00001001       9
du --help     00010001      17
*/



bool test_du(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="du"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数等于1
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x02;     //参数大于1
		if(cmd.get_file_no()>=1)FLAG_CMD |= 0x04;     //名字>0
		if(cmd.find_arg("-s"))FLAG_CMD |= 0x08;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x10;

		int size = 0;
		switch(FLAG_CMD){
		  case 0:	
			  {
				  	for(size_t i=0;i<INDEX_LENGTH;i++)
						for(size_t j=0;j<INDEX_NUM;j++){
							if(disk.d_index[i].fnode[j].id != 0 && disk.d_index[i].fnode[j].back ==back_folder){
								du(disk, disk.d_index[i].fnode[j].name , back_folder, size ,user , 0);
							}
						}
					break;
			  }
		  case 4:	for(int i = 0; i<cmd.get_file_no();i++)du(disk, cmd.get_file(i) , back_folder, size ,user , 0); break;
		  case 9:
			  {
				  	for(size_t i=0;i<INDEX_LENGTH;i++)
						for(size_t j=0;j<INDEX_NUM;j++){
							if(disk.d_index[i].fnode[j].id != 0 && disk.d_index[i].fnode[j].back ==back_folder){
								du(disk, disk.d_index[i].fnode[j].name , back_folder, size ,user , 1);
							}
						}
					break;
			  }
		  case 13:	for(int i = 0; i<cmd.get_file_no();i++)du(disk, cmd.get_file(i) , back_folder, size ,user , 1); break;
		  case 17:	cout<<"****************************\nHELP about du\n  Format: du\n\t du name1 name2\n\t du -s name\n****************************"<<endl;	 break;
		  default: cout<<"Incorrect format!\n";break;
		}
		return true;
	}
	else return false;
}



/****************************************************************************/
/************************************more***********************************/
/****************************************************************************/

#define MORE 8     //more命令 一页显示行数

bool more(DISK& disk, string name ,short back_folder , uchar user = 0, char flag = 0){        //FLAG = 0 原样显示  FLAG = 1 连续两行及以上空行只显示一行       
	/*
	检查文件是否存在  存在则继续

	打开文件并找到

	输出文件内容
	*/

	int state;   	
	string str_name ;
	str_name = get_name(disk, name , back_folder,state,user);          //先获得文件名和上级目录
	if(state == 0)return false;

	const char *STR = str_name.c_str(); 
	bool find = false;
	size_t c,d;              //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0
	ushort start ;             //标记该文件起始块
	uchar total , line ;              //保存文件块数和行数   line = (size - 32)/LINE_LENGTH
	string str[BLOCK_SIZE/LINE_LENGTH-1];                     //开辟第一个块的空间
	string str2[BLOCK_SIZE/LINE_LENGTH];                      //第二个块的空间备用

	if(back_folder == 0){		
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件
				if(disk.d_index[c].fnode[d].id>0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , STR)==0  ){  
					find = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
		start = disk.d_index[c].fnode[d].start_block;
	}
	else if(back_folder<0){
		get_index(back_folder,m,n);     //获得目录所在索引序号  m  n
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , STR)==0){      //找到同名的
					find = true;
					break;
				}
			}
		}
		start = disk.d_index[p].fnode[k].start_block ;
	}                                       

	if(!find){
		cout<<"Can't find the file "<<str_name<<" ,it doesn't exist!"<<endl;
		return false;
	}
	//至此已经找到了该文件

	total = disk.d_block[start].file.total ;
	line = (disk.d_block[start].file.size - 32)/32;
	char tempstr[32];

	int num = 0;

	int times = 0;
	string tmp_input;

	if(total == 1){
		size_t h=0;
		while(h<=line){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				if(flag==1){
					if(str[h]==""){
						num++;
						if(num>1);
						else cout<<str[h]<<endl;
					}
					else {
						num = 0;
						cout<<str[h]<<endl;
					}
				}
				else cout<<str[h]<<endl;
			}
			h++;  
			times++;
			if(times==MORE){
				do{
					getline(cin,tmp_input);
				}while(tmp_input!="");
				times = 0;
			}
		}
		
	}
	else if(total==2){
		size_t h=0, q=0;
		while(h<127){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				if(flag==1){
					if(str[h]==""){
						num++;
						if(num>1);
						else cout<<str[h]<<endl;
					}
					else {
						num = 0;
						cout<<str[h]<<endl;
					}
				}
				else cout<<str[h]<<endl;
			}
			h++;  
			times++;
			if(times==MORE){
				do{
					getline(cin,tmp_input);
				}while(tmp_input!="");
				times = 0;
			}
		}
		while(q<=line-127){
			strncpy(tempstr, &disk.d_block[start].file.Data[q*LINE_LENGTH], LINE_LENGTH );
			str2[q] = tempstr ;
			if(str2[q]!=":s" && str2[q]!=":S"){
				if(flag==1){
					if(str2[q]==""){
						num++;
						if(num>1);
						else cout<<str2[q]<<endl;
					}
					else {
						num = 0;
						cout<<str2[q]<<endl;
					}
				}
				else cout<<str2[q]<<endl;
			}
			q++;
			times++;
			if(times==MORE){
				do{
					getline(cin,tmp_input);
				}while(tmp_input!="");
				times = 0;
			}
		}
	}
	return true;
}

/*
more xxx         0010
more -s xxx     0111
more --help     1001
*/

bool test_more(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="more"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数为1 
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x80;     //参数大于1 
		if(cmd.get_file_no()==1)FLAG_CMD |= 0x02;    //名字==1
		if(cmd.find_arg("-s"))FLAG_CMD |= 0x04;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x08;

		switch(FLAG_CMD){
		  case 2:	more(disk,cmd.get_file(0) ,back_folder ,user,0);  break;
		  case 9:	cout<<"****************************\nHELP about more\n  Format: more name\n\t more -s name\n****************************"<<endl;	 break;
          case 7:	more(disk,cmd.get_file(0) ,back_folder ,user, 1);	  break;
		  default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}




/****************************************************************************/
/**********************************touch2**********************************/
/****************************************************************************/

bool touch2(DISK& disk, string name ,short back_folder , uchar user = 0,char flag = 0){   //flag==0 存在则提示  flag==1 存在则覆盖
	size_t i,j;                          // i  j 对应找到的空闲结点
	bool find =false, find2 = false;
	for(i=0;i<INDEX_LENGTH;i++){
		for(j=0;j<INDEX_NUM;j++){
			if(!disk.d_index[i].fnode[j].id){                     //找到空闲的索引结点 disk.d_index[i].fnode[j] 
				find = true;
				break;
			}
		}
		if(find==true)break;
	}

	//找到空闲的磁盘块   找不到 find2 = false
	size_t z;
	for(z=0;z<TABLE_LENGTH;z++){ 
		if(disk.d_table.state[z]==0){                            //找到磁盘块为z
			find2 = true;
			break;
		}
	}

	if(!find || !find2){
		cout<<"Disk is full now!\n";
		return false;
	}

	const char *str = name.c_str();
	size_t m,n,t, p,k;

	if(back_folder<0){     //不在根目录下
		if(!get_index(back_folder,m,n))     //获得当前目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件夹没   存在则修改时间
			if(disk.d_index[m].fnode[n].next[t]<0){    
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					if(flag==1){
						disk.d_index[p].fnode[k].time = time(NULL);
						return true;
					}  //不能覆盖 找到了同名文件  必须退出报错
					else{
						cout<<name<<" already exist!"<<endl;
						return false;       
					}
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					if(flag==1){
						disk.d_index[p].fnode[k].time = time(NULL);
						return true;
					}
					else {
						cout<<name<<" already exist!"<<endl;
						return false;
					}
				}
			}
		}

			//不存在同名文件 则新建
			if(name.size()<61){               //检查文件名和文件类型合法性
			//在上级目录下看满了没 如果没满就添加 满了报错 退出

				if(get_index(back_folder, p ,k)){

					for(t=0;t<INC_FILE;t++){
						if(disk.d_index[p].fnode[k].next[t]==0){
							disk.d_index[p].fnode[k].next[t] =  (i*INDEX_NUM+j+1);        //增加父节点的目录域next
							break;
						}
					}

					if(t==INC_FILE){cout<<"Reach the limit ,can't create!"<<endl;return false;}
				}

				//添加FNODE
				disk.d_index[i].fnode[j].id = (i*INDEX_NUM+j+1);   //索引盘块号×索引盘块数+（序号+1）  再取正号
				strcpy(disk.d_index[i].fnode[j].name ,str);
				disk.d_index[i].fnode[j].start_block = z;                   //起始盘号
				disk.d_index[i].fnode[j].back = back_folder;
				for(int t=0;t<INC_FILE;t++)
					disk.d_index[i].fnode[j].next[t] = 0;                      //此句无用
				disk.d_index[i].fnode[j].state = 0xFF;		
				disk.d_index[i].fnode[j].userid = user;  
				disk.d_index[i].fnode[j].time = time(NULL);
				
				//添加对应的磁盘块

				disk.d_block[z].file.size =32;                       //文件大小 空文件就是32B 
				disk.d_block[z].file.start = z ;                      //起始盘块
				disk.d_block[z].file.total = 1;                       //共占有盘块数 
				strcpy(disk.d_block[z].file.Data,"");
				
				//在table中记录该文件
				disk.d_table.state[z] = 1;
			}
			else {
				cout<<"Illegal name or type"<<endl;
				return false;
			}
		}

	
	

	else if(back_folder == 0){   //在根目录下
		if(name.size()<61){               //检查文件名和文件类型合法性
			//查找根目录下有没有同名文件
			size_t c,d;
			for(c=0;c<INDEX_LENGTH;c++){
				for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹     则修改时间
					if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){             
						if(flag==1){
							disk.d_index[p].fnode[k].time = time(NULL);
							return true;
						}
						else {
							cout<<name<<" already exist!"<<endl;
							return false;
						}
					}
				}
			}
			//找到同名文件报错 没有则创建文件

			//添加FNODE				
			disk.d_index[i].fnode[j].id = (i*INDEX_NUM+j+1);   //索引盘块号×索引盘块数+（序号+1）  再取正号
			strcpy(disk.d_index[i].fnode[j].name ,str);
			disk.d_index[i].fnode[j].start_block = z;                   //起始盘号
			disk.d_index[i].fnode[j].back = 0;
			for(int t=0;t<INC_FILE;t++)
				disk.d_index[i].fnode[j].next[t] = 0;                      //此句无用
			disk.d_index[i].fnode[j].state = 0xFF;		
			disk.d_index[i].fnode[j].userid = user;  
			disk.d_index[i].fnode[j].time = time(NULL);
			
			//添加对应的磁盘块 
			disk.d_block[z].file.size =32;                       //文件大小 空文件就是32B 
			disk.d_block[z].file.start = z ;                      //起始盘块
			disk.d_block[z].file.total = 1;                       //共占有盘块数 
			strcpy(disk.d_block[z].file.Data,"");
			
			//在table中记录该文件
			disk.d_table.state[z] = 1;

		}
		else {
			cout<<"Illegal name or type"<<endl;
			return false;
		}
	}
	return true;

}


/****************************************************************************/
/**************************************cp***********************************/
/****************************************************************************/

/*
cp xxx yyy
只接受两个参数 
如果是文件  直接touch一个yyy 再copy全部数据
如果是目录 mkdir yyy 再内部递归调用

*/

bool cp(DISK& disk, string src, string dst ,short back1, short back2 , uchar user = 0, char flag = 0){  //flag = 0 存在则不覆盖  1存在则覆盖   
	int state;
	src = get_name(disk, src , back1, state, user);         //获得源名字
	if(state!=1)return false;

	const char *str = src.c_str(); 
	bool find = false;
	bool dir ;
	size_t c,d;     //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0
	size_t C,D;
	size_t M,N,T,P,K;

	if(back1 == 0){	
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹
				if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){  
					find = true;
					if(disk.d_index[c].fnode[d].id>0)dir = false;
					else dir = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
	}
	else if(back1<0){
		if(!get_index(back1,m,n))     //获得目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = false;
					break;
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]<0){                //存在文件夹
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = true;
					break;
				}
			}
		}
	}

	if(!find){
		cout<<"File "<<src<<" doesn't exist!"<<endl;
		return false;
	}

	//至此 找到了源  

	if(dir){
		dst = get_name(disk, dst , back2, state, user); 

		const char *STR = dst.c_str(); 
		
		
		if(state==1 && flag == 0){
			if(!mkdir(disk, dst ,back2 ,user, 0))
				return false;
		}
		
		else if(flag==1)mkdir(disk, dst ,back2 ,user , 1);       

		find = false;

		if(back2 == 0){	                 //获得目标文件
			for(C=0;C<INDEX_LENGTH;C++){
				for(D=0;D<INDEX_NUM;D++){    //找同名文件or文件夹
					if(disk.d_index[C].fnode[D].id<0 && disk.d_index[C].fnode[D].back == 0  && strcmp(disk.d_index[C].fnode[D].name , STR)==0  ){  
						find = true;
						break;
					}
				}
				if(find == true)break;                 //找到同名文件or文件夹
			}
		}
		else if(back2<0){
			if(!get_index(back2,M,N))     //获得目录所在索引序号  m  n
				return false;

			for(T=0;T< INC_FILE ;T++){      //查找当前目录下存在同名文件没  
				if(disk.d_index[M].fnode[N].next[T]<0){                //存在文件夹
					P = (-disk.d_index[M].fnode[N].next[T]-1)/32;
					K = (-disk.d_index[M].fnode[N].next[T]-1)%32;
					if(strcmp(disk.d_index[P].fnode[K].name , STR)==0){      //找到同名的
						find = true;
						break;
					}
				}
			}
		}

		size_t u,v;

		if(back1 == 0 && back2 ==0){
			for(size_t l=0; l<INC_FILE;l++){
				if(disk.d_index[c].fnode[d].next[l]>0){
					get_index_2(disk.d_index[c].fnode[d].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name ,disk.d_index[c].fnode[d].id, disk.d_index[C].fnode[D].id , user); 
				}		
				else if(disk.d_index[c].fnode[d].next[l]<0){
					get_index(disk.d_index[c].fnode[d].next[l], u , v);
	
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name, disk.d_index[c].fnode[d].id ,disk.d_index[C].fnode[D].id , user); 
				}
			}
			return true;
		}

		else if(back1<0 && back2<0){
			for(size_t l=0; l<INC_FILE;l++){
				if(disk.d_index[p].fnode[k].next[l]>0){
					get_index_2(disk.d_index[p].fnode[k].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name ,disk.d_index[p].fnode[k].id ,disk.d_index[P].fnode[K].id , user); 
				}		
				else if(disk.d_index[p].fnode[k].next[l]<0){
					get_index(disk.d_index[p].fnode[k].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name,disk.d_index[p].fnode[k].id  ,disk.d_index[P].fnode[K].id , user); 
				}
			}
			return true;
		}

		else if(back1==0 && back2<0){
			for(size_t l=0; l<INC_FILE;l++){
				if(disk.d_index[c].fnode[d].next[l]>0){
					get_index_2(disk.d_index[c].fnode[d].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name ,disk.d_index[c].fnode[d].id,disk.d_index[P].fnode[K].id, user); 
				}		
				else if(disk.d_index[c].fnode[d].next[l]<0){
					get_index(disk.d_index[c].fnode[d].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name, disk.d_index[c].fnode[d].id ,disk.d_index[P].fnode[K].id, user); 
				}
			}
			return true;
		}

		else if(back1<0 && back2==0){
			for(size_t l=0; l<INC_FILE;l++){
				if(disk.d_index[p].fnode[k].next[l]>0){
					get_index_2(disk.d_index[p].fnode[k].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name,disk.d_index[p].fnode[k].id  ,disk.d_index[C].fnode[D].id , user); 
				}		
				else if(disk.d_index[p].fnode[k].next[l]<0){
					get_index(disk.d_index[p].fnode[k].next[l], u , v);
					cp(disk, disk.d_index[u].fnode[v].name , disk.d_index[u].fnode[v].name,disk.d_index[p].fnode[k].id  ,disk.d_index[C].fnode[D].id , user); 
				}
			}
			return true;
		}

	}
	else {
		find = false;
		dst = get_name(disk, dst , back2, state, user); 
		const char *STR = dst.c_str(); 

		if(flag==0 && state==1){
			if(!touch2(disk, dst ,back2 ,user,flag))
				return false;
		}
		else if(flag==1)
			touch(disk, dst ,back2 ,user,1);

		if(back2 == 0){	                 //获得目标文件
			for(C=0;C<INDEX_LENGTH;C++){
				for(D=0;D<INDEX_NUM;D++){    //找同名文件
					if(disk.d_index[C].fnode[D].id>0 && disk.d_index[C].fnode[D].back == 0  && strcmp(disk.d_index[C].fnode[D].name , STR)==0  ){  
						find = true;
						break;
					}
				}
				if(find == true)break;                 //找到同名文件or文件夹
			}
		}
		else if(back2<0){
			if(!get_index(back2,M,N))     //获得目录所在索引序号  m  n
				return false;
			for(T=0;T< INC_FILE ;T++){      //查找当前目录下存在同名文件没  
				if(disk.d_index[M].fnode[N].next[T]>0){                //存在文件
					P = (disk.d_index[M].fnode[N].next[T]-1)/32;
					K = (disk.d_index[M].fnode[N].next[T]-1)%32;
					if(strcmp(disk.d_index[P].fnode[K].name , STR)==0){      //找到同名的
						find = true;
						break;
					}
				}
			}
		}

		if(back1 == 0 && back2 ==0){
			if(disk.d_block[disk.d_index[c].fnode[d].start_block].file.total==1){
				disk.d_block[disk.d_index[C].fnode[D].start_block] = disk.d_block[disk.d_index[c].fnode[d].start_block];
			}
			else{
				disk.d_block[disk.d_index[C].fnode[D].start_block] = disk.d_block[disk.d_index[c].fnode[d].start_block];
				if(disk.d_table.state[disk.d_index[C].fnode[D].start_block+1] == 1){
					cout<<"Can't copy correctly !"<<endl;
					return false;
				}
				disk.d_table.state[disk.d_index[C].fnode[D].start_block+1] = 2 ;
				disk.d_block[disk.d_index[C].fnode[D].start_block+1] = disk.d_block[disk.d_index[c].fnode[d].start_block+1];
				return true;
			}
		}

		else if(back1<0 && back2<0){
			if(disk.d_block[disk.d_index[p].fnode[k].start_block].file.total==1){
				disk.d_block[disk.d_index[P].fnode[K].start_block] = disk.d_block[disk.d_index[p].fnode[k].start_block];
			}
			else{
				disk.d_block[disk.d_index[P].fnode[K].start_block] = disk.d_block[disk.d_index[p].fnode[k].start_block];
				if(disk.d_table.state[disk.d_index[P].fnode[K].start_block+1] == 1){
					cout<<"Can't copy correctly !"<<endl;
					return false;
				}
				disk.d_table.state[disk.d_index[P].fnode[K].start_block+1] = 2 ;
				disk.d_block[disk.d_index[P].fnode[K].start_block+1] = disk.d_block[disk.d_index[p].fnode[k].start_block+1];
				return true;
			}
		}

		else if(back1==0 && back2<0){
			if(disk.d_block[disk.d_index[c].fnode[d].start_block].file.total==1){
				disk.d_block[disk.d_index[P].fnode[K].start_block] = disk.d_block[disk.d_index[c].fnode[d].start_block];
			}
			else{
				disk.d_block[disk.d_index[P].fnode[K].start_block] = disk.d_block[disk.d_index[c].fnode[d].start_block];
				if(disk.d_table.state[disk.d_index[P].fnode[K].start_block+1] == 1){
					cout<<"Can't copy correctly !"<<endl;
					return false;
				}
				disk.d_table.state[disk.d_index[P].fnode[K].start_block+1] = 2 ;
				disk.d_block[disk.d_index[P].fnode[K].start_block+1] = disk.d_block[disk.d_index[c].fnode[d].start_block+1];
				return true;
			}
		}

		else if(back1<0 && back2==0){
			if(disk.d_block[disk.d_index[p].fnode[k].start_block].file.total==1){
				disk.d_block[disk.d_index[C].fnode[D].start_block] = disk.d_block[disk.d_index[p].fnode[k].start_block];
			}
			else{
				disk.d_block[disk.d_index[C].fnode[D].start_block] = disk.d_block[disk.d_index[p].fnode[k].start_block];
				if(disk.d_table.state[disk.d_index[C].fnode[D].start_block+1] == 1){
					cout<<"Can't copy correctly !"<<endl;
					return false;
				}
				disk.d_table.state[disk.d_index[C].fnode[D].start_block+1] = 2 ;
				disk.d_block[disk.d_index[C].fnode[D].start_block+1] = disk.d_block[disk.d_index[p].fnode[k].start_block+1];
				return true;
			}
		}
	}
	return true;
}


bool test_cp(Command& cmd,DISK& disk,short back_folder , uchar user = 0){
   if(cmd.get_name()=="cp"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_file_no()==2 && cmd.get_arg_no()==0)FLAG_CMD |= 0x00;
	  else if(cmd.get_file_no()==2 && cmd.get_arg_no()==1 && cmd.find_arg("-f"))FLAG_CMD |= 0x01;
	  else if(cmd.get_file_no()==0 && cmd.get_arg_no()==1 && cmd.find_arg("-help"))FLAG_CMD |= 0x02;
	  else FLAG_CMD |= 0x03;

      switch(FLAG_CMD){
          case 0:  cp(disk,cmd.get_file(0),cmd.get_file(1),back_folder,back_folder,user,0);  break;
		  case 1:  cp(disk,cmd.get_file(0),cmd.get_file(1),back_folder,back_folder,user,1);  break;
		  case 2:   cout<<"****************************\nHELP about cp\n  Format: cp src dst\n\tcp -f src dst\n****************************"<<endl;	 break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}


/****************************************************************************/
/**************************************mv************************************/
/****************************************************************************/

bool mv(DISK& disk, string src, string dst ,short back1, short back2 , uchar user = 0, char flag = 0){
	/*
	flag==0 不覆盖  flag ==1 覆盖
	*/
	if(flag==0)
		if(!cp(disk, src, dst ,back1, back2 , user , 0))
			return false;
	else
		if(cp(disk, src, dst ,back1, back2 , user ,1 ))
			return false;
	rm(disk,src, back1 , user ,1);
	return true;
}

bool test_mv(Command& cmd,DISK& disk,short back_folder , uchar user = 0){
   if(cmd.get_name()=="mv"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_file_no()==2 && cmd.get_arg_no()==0)FLAG_CMD |= 0x00;
	  else if(cmd.get_file_no()==2 && cmd.get_arg_no()==1 && cmd.find_arg("-f"))FLAG_CMD |= 0x01;
	  else if(cmd.get_file_no()==0 && cmd.get_arg_no()==1 && cmd.find_arg("-help"))FLAG_CMD |= 0x02;
	  else FLAG_CMD |= 0x03;

      switch(FLAG_CMD){
          case 0:  mv(disk,cmd.get_file(0),cmd.get_file(1),back_folder,back_folder,user,0);  break;
		  case 1:  mv(disk,cmd.get_file(0),cmd.get_file(1),back_folder,back_folder,user,1);  break;
		  case 2:   cout<<"****************************\nHELP about mv\n  Format: mv src dst\n\tmv -f src dst\n****************************"<<endl;	 break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}


/***************************************************************************/
/**************************************date*********************************/
/****************************************************************************/

void date(){
	time_t   lt;   
	lt=time(NULL);
	printf(ctime(&lt));
}

bool test_date(Command& cmd){
   if(cmd.get_name()=="date"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_file_no()==0 && cmd.get_arg_no()==0)FLAG_CMD |= 0x00;
	  else FLAG_CMD |= 0x03;

      switch(FLAG_CMD){
          case 0:  date();  break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}


/***************************************************************************/
/***********************************chmod*********************************/
/****************************************************************************/


bool chmod(DISK& disk, string src  ,short back_folder, uchar mode , uchar user = 0, char flag = 0){  //flag = 0 不递归  1 递归处理
	int state;
	src = get_name(disk, src , back_folder, state, user);         //获得源名字
	if(state!=1)return false;

	const char *str = src.c_str(); 
	bool find = false;
	bool dir ;
	size_t c,d;     //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0

	if(back_folder == 0){	
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹
				if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){  
					find = true;
					if(disk.d_index[c].fnode[d].id>0)dir = false;
					else dir = true;
					disk.d_index[c].fnode[d].state = mode;          //修改权限
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
	}
	else if(back_folder<0){
		if(!get_index(back_folder,m,n))     //获得目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = false;
					disk.d_index[p].fnode[k].state = mode;      //修改权限
					break;
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]<0){                //存在文件夹
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = true;
					disk.d_index[p].fnode[k].state = mode;      //修改权限
					break;
				}
			}
		}
	}

	if(!find){
		cout<<"File /Dir "<<src<<" doesn't exist!"<<endl;
		return false;
	}

	//至此 修改好了当前权限  现在需要递归处理目录下权限

	if(dir && flag ==1){
		size_t u,v;

		if(back_folder == 0){
			for(size_t l=0; l<INC_FILE;l++){
				if(disk.d_index[c].fnode[d].next[l]>0){
					get_index_2(disk.d_index[c].fnode[d].next[l], u , v);
					chmod(disk, disk.d_index[u].fnode[v].name ,disk.d_index[c].fnode[d].id, mode, user, 1); 
				}		
				else if(disk.d_index[c].fnode[d].next[l]<0){
					get_index(disk.d_index[c].fnode[d].next[l], u , v);
					chmod(disk, disk.d_index[u].fnode[v].name ,disk.d_index[c].fnode[d].id, mode, user, 1); 
				}
			}
			return true;
		}

		else if(back_folder<0){
			for(size_t l=0; l<INC_FILE;l++){
				if(disk.d_index[p].fnode[k].next[l]>0){
					get_index_2(disk.d_index[p].fnode[k].next[l], u , v);
					chmod(disk, disk.d_index[u].fnode[v].name ,disk.d_index[p].fnode[k].id ,mode , user ,1); 
				}		
				else if(disk.d_index[p].fnode[k].next[l]<0){
					get_index(disk.d_index[p].fnode[k].next[l], u , v);
					chmod(disk, disk.d_index[u].fnode[v].name ,disk.d_index[p].fnode[k].id ,mode , user ,1); 
				}
			}
			return true;
		}
	return true;
	}
}

#include "stdlib.h"

		/*                               --help    -R       1st =num &&name>=2    arg>1    arg=1
		chmod num name....           00000100   4
		chmod -R num name...       00001101   13
		chmod --help                     00010001   17
		*/

bool test_chmod(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="chmod"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数等于1
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x02;   //参数等于1
		if(cmd.get_file_no()>=2 && atoi(cmd.get_file(0).c_str() ) && atoi(cmd.get_file(0).c_str() )<=255 )FLAG_CMD |= 0x04;     //1st =num &&name>=2 
		if(cmd.find_arg("-R"))FLAG_CMD |= 0x08;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x10;

		uchar mode = atoi(cmd.get_file(0).c_str() );

		switch(FLAG_CMD){
		  case 4:	for(int i = 1; i<cmd.get_file_no();i++)chmod(disk, cmd.get_file(i) ,back_folder, mode ,user , 0); break;
		  case 13:	for(int i = 1; i<cmd.get_file_no();i++)chmod(disk, cmd.get_file(i) ,back_folder, mode ,user , 1); break;
		  case 17:	cout<<"****************************\nHELP about chmod\n  Format: chmod num name\n\t chmod -R num name1 name2\n\t du -s name\n****************************"<<endl;	 break;
		  default: cout<<"Incorrect format!\n";break;
		}
		return true;
	}
	else return false;
}


/****************************************************************************/
/**************************************grep***********************************/
/****************************************************************************/

bool str_compare(string str1, string str2){
	bool find = false;
	for(size_t i=0, j=0 ; i<str1.size();i++){
		if(str1[i]==str2[j]){
			if(j==(str2.size()-1)){
				find = true;
				break;
			}
			else j++;
		}
		else {
			j = 0;
		}
	}
	return find;
}


bool grep(DISK& disk, string name , string search ,short back_folder , uchar user = 0, char flag = 0){        //flag=1 显示行数：       
	int state;   	
	string str_name ;
	str_name = get_name(disk, name , back_folder,state,user);          //先获得文件名和上级目录
	if(state == 0)return false;

	const char *STR = str_name.c_str(); 
	bool find = false;
	size_t c,d;              //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0
	ushort start ;             //标记该文件起始块
	uchar total , line ;              //保存文件块数和行数   line = (size - 32)/LINE_LENGTH
	string str[BLOCK_SIZE/LINE_LENGTH-1];                     //开辟第一个块的空间
	string str2[BLOCK_SIZE/LINE_LENGTH];                      //第二个块的空间备用

	if(back_folder == 0){		
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件
				if(disk.d_index[c].fnode[d].id>0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , STR)==0  ){  
					find = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
		start = disk.d_index[c].fnode[d].start_block;
	}
	else if(back_folder<0){
		get_index(back_folder,m,n);     //获得目录所在索引序号  m  n
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , STR)==0){      //找到同名的
					find = true;
					break;
				}
			}
		}
		start = disk.d_index[p].fnode[k].start_block ;
	}                                       

	if(!find){
		cout<<"Can't find the file "<<str_name<<" ,it doesn't exist!"<<endl;
		return false;
	}
	//至此已经找到了该文件

	total = disk.d_block[start].file.total ;
	line = (disk.d_block[start].file.size - 32)/32;
	char tempstr[32];

	int num = 0;

	cout<<"In file "<<str_name<<" :  "<<endl;
	if(total == 1){
		size_t h=0;
		while(h<=line){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				if( str_compare(str[h], search)){
					if(flag==1)cout<<"  "<<h+1<<": ";
					cout<<"  "<<str[h]<<endl;
				}
			}
			h++;  
		}
		
	}
	else if(total==2){
		size_t h=0, q=0;
		while(h<127){
			strncpy(tempstr, &disk.d_block[start].file.Data[h*LINE_LENGTH], LINE_LENGTH );
			str[h] =tempstr;
			if(str[h]!=":s" && str[h]!=":S"){
				if(str_compare(str[h], search)){
					if(flag==1)cout<<"  "<<h+1<<": ";
					cout<<"  "<<str[h]<<endl;
				}
			}
			h++;  
		}
		while(q<=line-127){
			strncpy(tempstr, &disk.d_block[start].file.Data[q*LINE_LENGTH], LINE_LENGTH );
			str2[q] = tempstr ;
			if(str2[q]!=":s" && str2[q]!=":S"){
				if(str_compare(str2[q], search)){
					if(flag==1)cout<<"  "<<q+1<<": ";
					cout<<"  "<<str2[q]<<endl;
				}
			}
			q++;
		}
	}
	return true;
}

/*                                     name=0   --help  -n  name>=2  arg>1  arg==1
grep str  name1 name2        00000100   4
grep -n str name1 name2    00001101   13
grep --help                          00110001   49
*/


bool test_grep(Command& cmd, DISK& disk, short back_folder , uchar user = 0){
	if(cmd.get_name()=="grep"){
		unsigned char FLAG_CMD = 0x00;
		if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;   //参数为1 
		if(cmd.get_arg_no()>1)FLAG_CMD |= 0x02;     //参数大于1 
		if(cmd.get_file_no()>=2)FLAG_CMD |= 0x04;    //名字==1
		if(cmd.find_arg("-n"))FLAG_CMD |= 0x08;
		if(cmd.find_arg("--help"))FLAG_CMD |= 0x10;
		if(cmd.get_file_no()==0)FLAG_CMD |= 0x20;  

		switch(FLAG_CMD){
		  case 4:	for(size_t i=1;i<cmd.get_file_no();i++)grep(disk, cmd.get_file(i) ,cmd.get_file(0) ,back_folder, user, 0);  break;
		  case 49:	cout<<"****************************\nHELP about grep\n  Format: grep str name\n\tgrep -n str name\n****************************"<<endl;	 break;
          case 13:	for(size_t i=1;i<cmd.get_file_no();i++)grep(disk, cmd.get_file(i) ,cmd.get_file(0) ,back_folder, user,1);  break;
		  default: cout<<"Incorrect format!\n";break;
      }
      return true;
   }
   else return false;
}


/****************************************************************************/
/******************************************ln*********************************/
/****************************************************************************/

	/*
	查找文件or文件夹存在否
	查找有没有剩余的inode 
	建立新node
	把原始node除了id都赋值给它
	*/


bool ln(DISK& disk, string src, string dst ,short back1, short back2 , uchar user = 0){
	int state;
	src = get_name(disk, src , back1, state, user);         //获得源名字
	if(state!=1)return false;
	const char *str = src.c_str(); 
	dst = get_name(disk, dst , back2, state, user); 
	if(state!=1)return false;
	const char *STR = dst.c_str(); 

	bool find = false;
	bool dir ;
	size_t c,d;     //back_folder = 0
	size_t m,n,t, p,k;    //back_folder<0

	if(back1 == 0){	
		for(c=0;c<INDEX_LENGTH;c++){
			for(d=0;d<INDEX_NUM;d++){    //找同名文件or文件夹
				if(disk.d_index[c].fnode[d].id!=0 && disk.d_index[c].fnode[d].back == 0  && strcmp(disk.d_index[c].fnode[d].name , str)==0  ){  
					find = true;
					if(disk.d_index[c].fnode[d].id>0)dir = false;
					else dir = true;
					break;
				}
			}
			if(find == true)break;                 //找到同名文件or文件夹
		}
	}
	else if(back1<0){
		if(!get_index(back1,m,n))     //获得目录所在索引序号  m  n
			return false;
		for(t=0;t< INC_FILE ;t++){      //查找当前目录下存在同名文件没  
			if(disk.d_index[m].fnode[n].next[t]>0){                //存在文件
				p = (disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = false;
					break;
				}
			}
			else if(disk.d_index[m].fnode[n].next[t]<0){                //存在文件夹
				p = (-disk.d_index[m].fnode[n].next[t]-1)/32;
				k = (-disk.d_index[m].fnode[n].next[t]-1)%32;
				if(strcmp(disk.d_index[p].fnode[k].name , str)==0){      //找到同名的
					find = true;
					dir = true;
					break;
				}
			}
		}
	}

	if(!find){
		cout<<"File "<<src<<" doesn't exist!"<<endl;
		return false;
	}

	//至此 找到了要创建链接的文件or文件夹

	//下面搜索空闲节点
	size_t i,j;
	find =false;
	for(i=0;i<INDEX_LENGTH;i++){
		for(j=0;j<INDEX_NUM;j++){
			if(!disk.d_index[i].fnode[j].id){                    //找到空闲的索引结点 disk.d_index[i].fnode[j] 
				find = true;
				break;
			}
		}
		if(find==true)break;
	}

	if(!find){
		cout<<"Disk is full now!Can not make a link.\n";
		return false;
	}
	if(back1==0){
		strcpy(disk.d_index[i].fnode[j].name, STR );
		disk.d_index[i].fnode[j].start_block = disk.d_index[c].fnode[d].start_block;
		disk.d_index[i].fnode[j].back = back2;
		for(size_t g=0;g<INC_FILE;g++)
			disk.d_index[i].fnode[j].next[g] = disk.d_index[c].fnode[d].next[g];
		disk.d_index[i].fnode[j].state = disk.d_index[c].fnode[d].state;
		disk.d_index[i].fnode[j].userid = disk.d_index[c].fnode[d].userid;
		disk.d_index[i].fnode[j].time = time(NULL);
	}
	else if(back1<0){
		strcpy(disk.d_index[i].fnode[j].name, STR );
		disk.d_index[i].fnode[j].start_block = disk.d_index[p].fnode[k].start_block;
		disk.d_index[i].fnode[j].back = back2;
		for(size_t g=0;g<INC_FILE;g++)
			disk.d_index[i].fnode[j].next[g] = disk.d_index[p].fnode[k].next[g];
		disk.d_index[i].fnode[j].state = disk.d_index[p].fnode[k].state;
		disk.d_index[i].fnode[j].userid = disk.d_index[p].fnode[k].userid;
		disk.d_index[i].fnode[j].time = time(NULL);
    }

	if(dir)
		disk.d_index[i].fnode[j].id = -(i*INDEX_NUM+j+1);
	else
		disk.d_index[i].fnode[j].id = i*INDEX_NUM+j+1;

	return true;
}


//仅建立软连接  按照规矩还是加上-s参数   

/*          name>1 help -s name>0 arg=1
ln  -s       00010111   23
ln --help 00001001    9
*/


bool test_ln(Command& cmd,DISK& disk,short back_folder , uchar user = 0){
   if(cmd.get_name()=="ln"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_arg_no()==1)FLAG_CMD |= 0x01;
	  if(cmd.get_file_no()>0)FLAG_CMD |= 0x02;
      if(cmd.find_arg("-s"))FLAG_CMD |= 0x04;
	  if(cmd.find_arg("--help"))FLAG_CMD |= 0x08;
	   if(cmd.get_file_no()>1)FLAG_CMD |= 0x10;
      switch(FLAG_CMD){
          case 23:  for(size_t i=1;i<cmd.get_file_no();i++)ln(disk, cmd.get_file(0), cmd.get_file(i) ,back_folder , back_folder ,user);  break;
          case 9: cout<<"****************************\nHELP about ln\n  Format: ln link name\n****************************"<<endl;  break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}

/****************************************************************************/
/**************************************pwd*********************************/
/****************************************************************************/

string get_cur_path(DISK& disk,short no){
	size_t m,n; 
	string path("");
	string tmp;
	while(no!=0){
		get_index(no,m,n);
		tmp = disk.d_index[m].fnode[n].name;
		path = "/"+ tmp + path;
		no = disk.d_index[m].fnode[n].back;
	}
	return path;
	
}

bool test_pwd(Command& cmd,DISK& disk,short back_folder , uchar user = 0){
   if(cmd.get_name()=="pwd"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_arg_no()>0)FLAG_CMD |= 0x01;
	  if(cmd.get_file_no()>0)FLAG_CMD |= 0x02;

      switch(FLAG_CMD){
          case 0: cout<<get_cur_path(disk,back_folder)<<endl ;  break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}


void show(DISK disk)
{
	for(size_t i=0;i<INDEX_LENGTH;i++)
		for(size_t j=0;j<INDEX_NUM;j++){
			if(disk.d_index[i].fnode[j].id != 0){
				cout<<"id = "<<disk.d_index[i].fnode[j].id<<" name = "<<disk.d_index[i].fnode[j].name<<"  father = "<<disk.d_index[i].fnode[j].back<<" "<<endl;
				cout<<"next = ";
				for(size_t m=0;m<INC_FILE;m++)
					cout<<disk.d_index[i].fnode[j].next[m]<<" ";
				cout<<endl;
			}
		}
}


bool test_show(Command& cmd,DISK& disk){
   if(cmd.get_name()=="show"){
      unsigned char FLAG_CMD = 0x00;
	  if(cmd.get_arg_no()>0)FLAG_CMD |= 0x01;
	  if(cmd.get_file_no()>0)FLAG_CMD |= 0x02;

      switch(FLAG_CMD){
          case 0: show(disk) ;  break;
          default: cout<<"Incorrect format!"<<endl;break;
      }
      return true;
   }
   else return false;
}