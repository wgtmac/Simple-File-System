#include "iostream"
#include "stdio.h"
using namespace std;
#include<fcntl.h>

#define uint unsigned int
#define uchar unsigned char
#define ushort unsigned short

#include "string.h"
#include "../include/cmd.h"
#include "../include/file_sys.h"

int main(){
	DISK disk;
	initialization(disk);

	int fd = open("/tmp/disk.txt",O_RDWR );           //读取磁盘数据
	read(fd, &disk, DISK_SIZE);
	close(fd);

	short cur_user = 0;
	short cur_path = 0;
	size_t m,n; 

	//验证用户名和密码
	char times = 5;
	string username,password;
	cout<<"Linux shell beta 1.0 version\nlogin : ";
	cin>>username;
	while(username!="wgtmac"){
		clear();
		if(times==0){
			cout<<"You have missed 5 times ! Now shut down\n";
			return 0;
		}
		cout<<"User "<<username<<" doesn't exist! Please retype in\nlogin : ";
		cin>>username;
		times--;
	}
	cout<<"Password : ";
	cin>>password;
	while(password!="wgtmac"){
		clear();
		if(times==0){
			cout<<"You have missed 5 times ! Now shut down\n";
			return 0;
		}
		cout<<"Incorrect password! Please retype in\nlogin : ";
		cout<<"Password : ";
		cin>>password;
		times--;
	}
	cin.get();


	while(1){
		if(cur_path!=0){
			cout<<username<<"@"<<username<<":"<<get_cur_path(disk,cur_path)<<"$ ";
		}
		else 
			cout<<username<<"@"<<username<<":/$ ";
		string STR;
		getline(cin,STR);
		Command cmd(STR);
		cmd.analyze();
		//cmd.print_arg();
		//cmd.print_file();

		if(test_cd(cmd,disk,cur_path,cur_user));
	    else if(test_ls(cmd,disk,cur_path,cur_user));
		else if(test_mkdir(cmd,disk,cur_path,cur_user));
		else if(test_rm(cmd,disk,cur_path,cur_user));
		else if(test_rmdir(cmd,disk,cur_path,cur_user));
		else if(test_clear(cmd))clear();
		else if(test_vi(cmd,disk, cur_path,cur_user));
		else if(test_touch(cmd,disk,cur_path,cur_user));
		else if(test_cat(cmd,disk,cur_path,cur_user));
		else if(test_more(cmd,disk,cur_path,cur_user));
		else if(test_wc(cmd,disk,cur_path,cur_user));
		else if(test_df(cmd,disk,cur_path,cur_user));
		else if(test_du(cmd,disk,cur_path,cur_user));
		else if(test_cp(cmd,disk,cur_path,cur_user));
		else if(test_mv(cmd,disk,cur_path,cur_user));
		else if(test_chmod(cmd,disk,cur_path,cur_user));
		else if(test_grep(cmd,disk,cur_path,cur_user));
		else if(test_ln(cmd,disk,cur_path,cur_user));
		else if(test_pwd(cmd,disk,cur_path,cur_user));
		else if(test_date(cmd));
		else if(test_show(cmd,disk));
		else if(test_exit(cmd)){
			int fd = open("/tmp/disk.txt",O_RDWR | O_CREAT );       //保存磁盘数据
			write(fd, &disk, DISK_SIZE);
			close(fd);
			return 0;
		}
	}

	return 0;
}


