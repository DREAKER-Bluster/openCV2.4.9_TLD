#include <opencv2/opencv.hpp>
#include <opencv2\core\core.hpp>
#include <opencv2\highgui\highgui.hpp>
#include "TLD.h"
#include "tld_utils.h"
#include<cstdio>
#include<omp.h>
#include <sstream>  //c++中的sstream类，提供了程序和string对象之间的I/O，可以通过ostringstream
//和instringstream两个类来声明对象，分别对应输出流和输入流
#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <windows.h>
using namespace cv;
using namespace std;

#pragma comment( lib, "IlmImfd.lib")
#pragma comment( lib, "libjasperd.lib")
#pragma comment( lib, "libjpegd.lib")
#pragma comment( lib, "libpngd.lib")
#pragma comment( lib, "libtiffd.lib")
#pragma comment( lib, "zlibd.lib")

#pragma comment( lib, "opencv_core249.lib")
#pragma comment( lib, "opencv_highgui249.lib")

#pragma comment( lib, "vfw32.lib" )  
#pragma comment( lib, "comctl32.lib" )

/*官方例程的*/
#define T_ANGLE_THRE 10
#define T_SIZE_THRE 5


int vmin = 10, vmax = 256, smin = 30;
int trackObject = 0;
bool backprojMode = false;
bool selectObject = false;
bool showHist = true;
bool b_leftflag = false, b_rightflag = false;

int g_left = 0, g_right = 0;

Rect selection = { 0,0,0,0 };
int dected = 0;
unsigned int frames1 = 0;
using namespace cv;
using namespace std;
Mat frame;
CvCapture* pCapture = NULL;
//VideoCapture cap("RedCar.avi");
VideoCapture cap(0);
//Global variables
Rect box;
//vector<Rect> temp;
bool drawing_box = false;
bool gotBB = false;
bool tl = true;
bool rep = false;
bool fromfile = false;   //此处修改过
						 //bool fromfile = true;   //此处修改过
string video;

HANDLE hComm;
OVERLAPPED OverLapped;
COMSTAT Comstat;
DWORD dwCommEvents;
int fps = (int)cvGetCaptureProperty(pCapture, CV_CAP_PROP_FPS);
//读取记录bounding box的文件，获得bounding box的四个参数：左上角坐标x，y和宽高
/*如在\datasets\06_car\init.txt中：记录了初始目标的bounding box，内容如下
142,125,232,164
*/


void brightAdjust(Mat &src, Mat &dst, double dContrast, double dBright)
{
	int nVal;
	int rowNumber = dst.rows;
	int colNumber = dst.cols*dst.channels();

	omp_set_num_threads(8);
#pragma omp parallel for

	for (int i = 0; i < rowNumber; i++)
	{
		uchar* dstdata = dst.ptr<uchar>(i);
		uchar* srcdata = src.ptr<uchar>(i);
		for (int j = 0; j < colNumber; j++)
		{
			nVal = ((dContrast * srcdata[j]) + dBright);
			if (nVal > 255) nVal = 255;
			else if (nVal < 0) nVal = 0;
			dstdata[j] = nVal;
		}
	}
}
void getDiffImage(Mat &src1, Mat &src2, Mat &dst, int nThre)
{
	int nVal;
	int rowNumber = src1.rows;
	int colNumber = src1.cols * src1.channels();

	omp_set_num_threads(8);
#pragma omp parallel for

	for (int i = 0; i < rowNumber; i++)
	{
		uchar* srcData1 = src1.ptr<uchar>(i);
		uchar* srcData2 = src2.ptr<uchar>(i);
		uchar* dstData = dst.ptr<uchar>(i);
		for (int j = 0; j < colNumber; j++)
		{
			if (srcData1[j] - srcData2[j]> nThre)
				dstData[j] = 255;
			else
				dstData[j] = 0;
		}
	}
}
int count_d = 0;
vector<RotatedRect> armorDetect(vector<RotatedRect> vEllipse)
{
	vector<RotatedRect> vRlt;
	RotatedRect armor;
	int nL, nW;
	double dAngle;
	vRlt.clear();
	if (vEllipse.size() < 2)
		return vRlt;
	for (unsigned int nI = 0; nI < vEllipse.size() - 1; nI++)
	{
		for (unsigned int nJ = nI + 1; nJ < vEllipse.size(); nJ++)
		{
			dAngle = abs(vEllipse[nI].angle - vEllipse[nJ].angle);
			while (dAngle > 180)
				dAngle -= 180;
			if ((dAngle < T_ANGLE_THRE || 180 - dAngle < T_ANGLE_THRE) && abs(vEllipse[nI].size.height - vEllipse[nJ].size.height) < (vEllipse[nI].size.height + vEllipse[nJ].size.height) / T_SIZE_THRE && abs(vEllipse[nI].size.width - vEllipse[nJ].size.width) < (vEllipse[nI].size.width + vEllipse[nJ].size.width) / T_SIZE_THRE)
			{
				selection.x = min(vEllipse[nI].center.x - vEllipse[nI].size.width / 2, vEllipse[nJ].center.x - vEllipse[nJ].size.width / 2);
				selection.y = min(vEllipse[nI].center.y - vEllipse[nI].size.height / 2, vEllipse[nJ].center.y - vEllipse[nJ].size.height / 2);
				selection.height = fabs(vEllipse[nI].size.height - vEllipse[nJ].size.height) + (vEllipse[nI].size.height + vEllipse[nJ].size.height) / 2;
				selection.width = fabs(vEllipse[nI].center.x - vEllipse[nJ].center.x) + (vEllipse[nI].size.width + vEllipse[nJ].size.width) / 2;

				drawBox(frame, selection);
				count_d++;
				if (count_d >5)
				{
					dected = 1;
				}

				armor.center.x = (vEllipse[nI].center.x + vEllipse[nJ].center.x) / 2;
				armor.center.y = (vEllipse[nI].center.y + vEllipse[nJ].center.y) / 2;
				armor.angle = (vEllipse[nI].angle + vEllipse[nJ].angle) / 2;
				if (180 - dAngle < T_ANGLE_THRE)
					armor.angle += 90;
				nL = (vEllipse[nI].size.height + vEllipse[nJ].size.height) / 2;
				nW = sqrt((vEllipse[nI].center.x - vEllipse[nJ].center.x) * (vEllipse[nI].center.x - vEllipse[nJ].center.x) + (vEllipse[nI].center.y - vEllipse[nJ].center.y) * (vEllipse[nI].center.y - vEllipse[nJ].center.y));
				if (nL < nW)
				{
					armor.size.height = nL;
					armor.size.width = nW;
				}
				else
				{
					armor.size.height = nW;
					armor.size.width = nL;
				}
				vRlt.push_back(armor);
			}
		}
	}
	return vRlt;
}
void drawbox(RotatedRect box, Mat &img)
{
	Point2f vertex[4];
	box.points(vertex);
	for (int i = 0; i < 4; i++)
	{
		line(img, vertex[i], vertex[(i + 1) % 4], Scalar(0, 0, 255), 2, CV_AA);
	}
}
int demo()
{
	bool bFlag = true;
	double dAngle;
	vector<Mat> channels;
	vector<RotatedRect> vEllipse;
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	vector<RotatedRect> vRlt;
	RotatedRect box;//旋转矩形
					//vector<Rect> temp;
	Point2f vertex[4];
	Mat bImage, gImage, rImage, rawImage, grayImage, rlt;
	Mat binary;
	//VideoCapture cap("RedCar.avi");
	cap >> frame;
	frame.copyTo(rawImage);
	double time = 0;

	while (!dected)
	{
		cap >> frame;
		frames1++;
		double t0 = getTickCount();
		brightAdjust(frame, rawImage, 1, -120);

		split(rawImage, channels);
		bImage = channels.at(0);
		gImage = channels.at(1);
		rImage = channels.at(2);
		bImage.copyTo(binary);
		getDiffImage(rImage, gImage, binary, 25);
		Mat element = getStructuringElement(MORPH_RECT, Size(5, 5));
		dilate(binary, grayImage, element, Point(-1, -1), 3);
		erode(grayImage, rlt, element, Point(-1, -1), 1);
		findContours(rlt, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);
		if (contours.size() == 0)
			continue;
		for (int i = 0; i < contours.size(); i++)
		{
			if ((contourArea(contours[i])>300 && contours.size() > 10) || (contours.size()<10 && contours.size()>30))
			{
				box = minAreaRect(Mat(contours[i])); //原代码是用椭圆拟合，然而我的电脑上fitellipse()出错
				box.points(vertex);
				for (int i = 0; i < 4; i++)
					line(rlt, vertex[i], vertex[(i + 1) % 4], Scalar(255), 2, CV_AA);

				if ((box.size.height / box.size.width) > 1.0)
					bFlag = true;
				for (int nI = 0; nI < 5; nI++)
				{
					for (int nJ = 0; nJ < 5; nJ++)
					{
						if (box.center.y - 2 + nJ > 0 && box.center.y - 2 + nJ < 480 && box.center.x - 2 + nI > 0 && box.center.x - 2 + nI < 640)
						{
							Vec3b sx = frame.at<Vec3b>((int)(box.center.y - 2 + nJ), (int)(box.center.x - 2 + nI));
							if (sx[0] < 200 || sx[1] < 200 || sx[2] < 200)
							{
								int x1 = sx[0];
								int x2 = sx[1];
								int x3 = sx[2];
								bFlag = false;
								break;
							}
						}
					}
				}
				if (bFlag)
				{
					vEllipse.push_back(box);
				}
			}
		}

		vRlt = armorDetect(vEllipse);
		for (unsigned int nI = 0; nI < vRlt.size(); nI++)
			drawbox(vRlt[nI], frame);
		//int flage = 0;
		//for (unsigned int nI = 0; nI < vRlt.size() - 1; nI++)
		//{
		//	for (unsigned int nJ = nI + 1; nJ < vRlt.size(); nJ++)
		//	{
		//		dAngle = abs(vRlt[nI].angle - vRlt[nJ].angle);
		//		while (dAngle > 180)
		//			dAngle -= 180;
		//		if ((dAngle < T_ANGLE_THRE || 180 - dAngle < T_ANGLE_THRE) && abs(vRlt[nI].size.height - vRlt[nJ].size.height) < (vRlt[nI].size.height + vRlt[nJ].size.height) / T_SIZE_THRE && abs(vRlt[nI].size.width - vRlt[nJ].size.width) < (vRlt[nI].size.width + vRlt[nJ].size.width) / T_SIZE_THRE)
		//		{
		//			flage++;
		//			temp[flage].x = min(vRlt[nI].center.x - vRlt[nI].size.width / 2, vRlt[nJ].center.x - vRlt[nJ].size.width / 2);
		//			temp[flage].y = min(vRlt[nI].center.y - vRlt[nI].size.height / 2, vRlt[nJ].center.y - vRlt[nJ].size.height / 2);
		//			temp[flage].height = fabs(vRlt[nI].size.height - vRlt[nJ].size.height) + (vRlt[nI].size.height + vRlt[nJ].size.height) / 2;
		//			temp[flage].width = fabs(vRlt[nI].center.x - vRlt[nJ].center.x) + (vRlt[nI].size.width + vRlt[nJ].size.width) / 2;
		//		}
		//	}
		//}
		//if (temp.size()>0)//若size大于0证明这个之中已经找到目标
		//{
		//	break;
		//}
		vEllipse.clear();
		vRlt.clear();


		imshow("Bi", binary);
		imshow("RLT", rlt);
		//imshow("frame", frame);
		imshow("TLD", frame);

		cvWaitKey(30);

		time += (getTickCount() - t0) / getTickFrequency();
		cout << frames1 / time << " fps" << endl;
	}

	return 0;
}



void readBB(char* file)
{
	ifstream bb_file(file);  //以输入方式打开文件
	string line;
	//istream& getline ( istream& , string& );
	//将输入流is中读到的字符存入str中，终结符默认为 '\n'（换行符）
	getline(bb_file, line);
	istringstream linestream(line); //istringstream对象可以绑定一行字符串，然后以空格为分隔符把该行分隔开来。
	string x1, y1, x2, y2;

	//istream& getline ( istream &is , string &str , char delim );
	//将输入流is中读到的字符存入str中，直到遇到终结符delim才结束。
	getline(linestream, x1, ',');
	getline(linestream, y1, ',');
	getline(linestream, x2, ',');
	getline(linestream, y2, ',');

	//atoi 功 能： 把字符串转换成整型数
	int x = atoi(x1.c_str());// = (int)file["bb_x"];
	int y = atoi(y1.c_str());// = (int)file["bb_y"];
	int w = atoi(x2.c_str()) - x;// = (int)file["bb_w"];
	int h = atoi(y2.c_str()) - y;// = (int)file["bb_h"];
	box = Rect(x, y, w, h);
}

//bounding box mouse callback
//鼠标的响应就是得到目标区域的范围，用鼠标选中bounding box。
//void mouseHandler(int event, int x, int y, int flags, void *param)
//{
//	switch (event)
//	{
//	case CV_EVENT_MOUSEMOVE:
//		if (drawing_box)
//		{
//			box.width = x - box.x;
//			box.height = y - box.y;
//		}
//		break;
//	case CV_EVENT_LBUTTONDOWN:
//		drawing_box = true;
//		box = Rect(x, y, 0, 0);
//		break;
//	case CV_EVENT_LBUTTONUP:
//		drawing_box = false;
//		if (box.width < 0)
//		{
//			box.x += box.width;
//			box.width *= -1;
//		}
//		if (box.height < 0)
//		{
//			box.y += box.height;
//			box.height *= -1;
//		}
//		gotBB = true;   //已经获得bounding box
//		break;
//	}
//}

void print_help(char** argv)
{
	//printf("use:\n     %s -p /path/parameters.yml\n",argv[0]);
	//printf("-s    source video\n-b        bounding box file\n-tl  track and learn\n-r     repeat\n");
	printf("\n video's fps = %d\t", fps);
}

//分析运行程序时的命令行参数
void read_options(int argc, char** argv, VideoCapture& capture, FileStorage &fs)
{
	for (int i = 0; i<argc; i++)
	{
		if (strcmp(argv[i], "-b") == 0)
		{
			if (argc>i)
			{
				readBB(argv[i + 1]);  //是否指定初始的bounding box
				gotBB = true;
			}
			else
				print_help(argv);
		}
		if (strcmp(argv[i], "-s") == 0)    //从视频文件中读取
		{
			if (argc>i)
			{
				video = string(argv[i + 1]);
				capture.open(video);
				fromfile = true;
			}
			else
				print_help(argv);

		}
		//Similar in format to XML, Yahoo! Markup Language (YML) provides functionality to Open
		//Applications in a safe and standardized fashion. You include YML tags in the HTML code
		//of an Open Application.
		if (strcmp(argv[i], "-p") == 0)    //读取参数文件parameters.yml
		{
			if (argc>i)
			{
				//FileStorage类的读取方式可以是：FileStorage fs(".\\parameters.yml", FileStorage::READ);
				fs.open(argv[i + 1], FileStorage::READ);
			}
			else
				print_help(argv);
		}
		if (strcmp(argv[i], "-no_tl") == 0)   //To train only in the first frame (no tracking, no learning)
		{
			tl = false;
		}
		if (strcmp(argv[i], "-r") == 0)   //Repeat the video, first time learns, second time detects
		{
			rep = true;
		}
	}
}

bool OpenPort()
{
	hComm = CreateFileW(L"COM7",
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		0);
	if (hComm == INVALID_HANDLE_VALUE)
		return FALSE;
	else
		return true;
}

bool SetupDCB(int rate_arg)
{
	DCB dcb;
	memset(&dcb, 0, sizeof(dcb));
	if (!GetCommState(hComm, &dcb))//获取当前DCB配置
	{
		return FALSE;
	}
	dcb.DCBlength = sizeof(dcb);
	/* ---------- Serial Port Config ------- */
	dcb.BaudRate = rate_arg;
	dcb.Parity = NOPARITY;
	dcb.fParity = 0;
	dcb.StopBits = ONESTOPBIT;
	dcb.ByteSize = 8;
	dcb.fOutxCtsFlow = 0;
	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = 0;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fOutX = 0;
	dcb.fInX = 0;
	dcb.fErrorChar = 0;
	dcb.fBinary = 1;
	dcb.fNull = 0;
	dcb.fAbortOnError = 0;
	dcb.wReserved = 0;
	dcb.XonLim = 2;
	dcb.XoffLim = 4;
	dcb.XonChar = 0x13;
	dcb.XoffChar = 0x19;
	dcb.EvtChar = 0;
	if (!SetCommState(hComm, &dcb))
	{
		return false;
	}
	else
		return true;
}

bool SetupTimeout(DWORD ReadInterval, DWORD ReadTotalMultiplier, DWORD
	ReadTotalConstant, DWORD WriteTotalMultiplier, DWORD WriteTotalConstant)
{
	COMMTIMEOUTS timeouts;
	timeouts.ReadIntervalTimeout = ReadInterval;
	timeouts.ReadTotalTimeoutConstant = ReadTotalConstant;
	timeouts.ReadTotalTimeoutMultiplier = ReadTotalMultiplier;
	timeouts.WriteTotalTimeoutConstant = WriteTotalConstant;
	timeouts.WriteTotalTimeoutMultiplier = WriteTotalMultiplier;
	if (!SetCommTimeouts(hComm, &timeouts))
	{
		return false;
	}
	else
		return true;
}

void ReciveChar()
{
	bool bRead = TRUE;
	bool bResult = TRUE;
	DWORD dwError = 0;
	DWORD BytesRead = 0;
	char RXBuff;
	for (;;)
	{
		bResult = ClearCommError(hComm, &dwError, &Comstat);
		if (Comstat.cbInQue == 0)
			continue;
		if (bRead)
		{
			bResult = ReadFile(hComm,  //通信设备（此处为串口）句柄，由CreateFile()返回值得到
				&RXBuff,  //指向接收缓冲区
				1,  //指明要从串口中读取的字节数
				&BytesRead,   //
				&OverLapped);  //OVERLAPPED结构
			std::cout << RXBuff << std::endl;
			if (!bResult)
			{
				switch (dwError == GetLastError())
				{
				case ERROR_IO_PENDING:
					bRead = FALSE;
					break;
				default:
					break;
				}
			}
		}
		else
		{
			bRead = TRUE;
		}
	}
	if (!bRead)
	{
		bRead = TRUE;
		bResult = GetOverlappedResult(hComm,
			&OverLapped,
			&BytesRead,
			TRUE);
	}
}

bool WriteChar(char* szWriteBuffer, DWORD dwSend)
{
	bool bWrite = TRUE;
	bool bResult = TRUE;
	DWORD BytesSent = 0;
	HANDLE hWriteEvent = NULL;
	ResetEvent(hWriteEvent);
	if (bWrite)
	{
		OverLapped.Offset = 0;
		OverLapped.OffsetHigh = 0;
		bResult = WriteFile(hComm,  //通信设备句柄，CreateFile()返回值得到
			szWriteBuffer,  //指向写入数据缓冲区
			dwSend,  //设置要写的字节数
			&BytesSent,  //
			&OverLapped);  //指向异步I/O数据
		if (!bResult)
		{
			DWORD dwError = GetLastError();
			switch (dwError)
			{
			case ERROR_IO_PENDING:
				BytesSent = 0;
				bWrite = FALSE;
				break;
			default:
				break;
			}
		}
	}
	if (!bWrite)
	{
		bWrite = TRUE;
		bResult = GetOverlappedResult(hComm,
			&OverLapped,
			&BytesSent,
			TRUE);
		if (!bResult)
		{
			std::cout << "GetOverlappedResults() in WriteFile()" << std::endl;
		}
	}
	if (BytesSent != dwSend)
	{
		std::cout << "WARNING: WriteFile() error.. Bytes Sent:" << BytesSent << "; Message Length: " << strlen((char*)szWriteBuffer) << std::endl;
	}
	return TRUE;
}



/*
运行程序时：
%To run from camera
./run_tld -p ../parameters.yml
%To run from file
./run_tld -p ../parameters.yml -s ../datasets/06_car/car.mpg
%To init bounding box from file
./run_tld -p ../parameters.yml -s ../datasets/06_car/car.mpg -b ../datasets/06_car/init.txt
%To train only in the first frame (no tracking, no learning)
./run_tld -p ../parameters.yml -s ../datasets/06_car/car.mpg -b ../datasets/06_car/init.txt -no_tl
%To test the final detector (Repeat the video, first time learns, second time detects)
./run_tld -p ../parameters.yml -s ../datasets/06_car/car.mpg -b ../datasets/06_car/init.txt -r
*/
//感觉就是对起始帧进行初始化工作，然后逐帧读入图片序列，进行算法处理。
int main()
{
	VideoCapture capture;
	capture.open(0);
	//capture.open(1);
	float fps;//计算fps零时变量
	double t = 0;//计算fps的

	char str[5];
	//OpenCV的C++接口中，用于保存图像的imwrite只能保存整数数据，且需作为图像格式。当需要保存浮
	//点数据或XML/YML文件时，OpenCV的C语言接口提供了cvSave函数，但这一函数在C++接口中已经被删除。
	//取而代之的是FileStorage类。
	FileStorage fs;
	//Read options

	//read_options(argc, argv, capture, fs);
	fs.open("parameters.yml", FileStorage::READ);	//分析命令行参数
													//Init camera

	if (!cap.isOpened())
	{
		cout << "cap device failed to open!" << endl;
		return 1;
	}
	//Register mouse callback to draw the bounding box

	//cvNamedWindow("TLD", CV_WINDOW_AUTOSIZE);
	//cvSetMouseCallback("TLD", mouseHandler, NULL); //用鼠标选中初始目标的bounding box


	//TLD framework
	TLD tld;
	//Read parameters file
	tld.read(fs.getFirstTopLevelNode());
	//Mat frame;
	Mat last_gray;
	Mat first;
	Rect tt;
	if (fromfile)   //如果指定为从文件读取
	{
		cap >> frame;   //读当前帧
		cvtColor(frame, last_gray, CV_RGB2GRAY);  //转换为灰度图像
		frame.copyTo(first);  //拷贝作为第一帧
	}
	else     //如果为读取摄像头，则设置获取的图像大小为320x240
	{
		cap.set(CV_CAP_PROP_FRAME_WIDTH, 320); //320
		cap.set(CV_CAP_PROP_FRAME_HEIGHT, 240);//240
	}
	if (OpenPort())
		std::cout << "Open port success" << std::endl;
	if (SetupDCB(9600))
		std::cout << "Set DCB success" << std::endl;
	if (SetupTimeout(0, 0, 0, 0, 0))
		std::cout << "Set timeout success" << std::endl;
	waitKey(25);
	PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
	WriteChar("Please send data:", 20);
	std::cout << "Send data:";
	//WriteChar("opencv2.4.9 is OK\n\t", 30);
	//ReciveChar();
	///Initialization
GETBOUNDINGBOX:   //标号：获取bounding box
	while (!gotBB)
	{
		t = (double)getTickCount();
		if (!fromfile)
		{
			cap >> frame;
			if (frame.empty())	 return 1;
			cvWaitKey(25);
		}
		else
			first.copyTo(frame);
		cvtColor(frame, last_gray, CV_RGB2GRAY);
		demo();
		//box = temp[1];
		box = selection;
		gotBB = 1;
		drawBox(frame, box);  //把bounding box 画出来

							  //t = ((double)getTickCount() - t) / getTickFrequency();
							  //fps = 1.0 / t;
							  //sprintf(str, "%3f", fps);
							  //string fpsStr("FPS:");
							  //fpsStr += str;
							  //putText(frame, fpsStr, Point(0, 20), FONT_HERSHEY_DUPLEX, 0.7, Scalar(0, 250, 205));
							  //putText(frame, fpsStr, Point(0, 30), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0, 50, 215));

		imshow("TLD", frame);
		if (cvWaitKey(5) == 'q')
			return 0;
	}
	//由于图像片（min_win 为15x15像素）是在bounding box中采样得到的，所以box必须比min_win要大
	if (min(box.width, box.height)<(int)fs.getFirstTopLevelNode()["min_win"])
	{
		cout << "Bounding box too small, try again." << endl;
		gotBB = false;
		goto GETBOUNDINGBOX;
	}
	//Remove callback
	//cvSetMouseCallback( "TLD", NULL, NULL );  //如果已经获得第一帧用户框定的box了，就取消鼠标响应

	printf("Initial Bounding Box = x:%d y:%d h:%d w:%d\n", box.x, box.y, box.width, box.height);
	//Output file
	FILE  *bb_file = fopen("bounding_boxes.txt", "w");

	//TLD initialization
	tld.init(last_gray, box, bb_file);

	///Run-time
	Mat current_gray;
	BoundingBox pbox;
	vector<Point2f> pts1;
	vector<Point2f> pts2;
	bool status = true;  //记录跟踪成功与否的状态 lastbox been found
	int frames = 1;  //记录已过去帧数
	int detections = 1;  //记录成功检测到的目标box数目

REPEAT:
	cap.set(CV_CAP_PROP_POS_FRAMES, frames1);
	cap >> frame;
	while (!frame.empty())
	{

		t = (double)getTickCount();
		//get frame
		cvtColor(frame, current_gray, CV_RGB2GRAY);
		//Process Frame
		tld.processFrame(last_gray, current_gray, pts1, pts2, pbox, status, tl, bb_file);
		//Draw Points

		if (status)   //如果跟踪成功
		{
			//drawPoints(frame, pts1);
			//drawPoints(frame, pts2, Scalar(0, 255, 0));  //当前的特征点用蓝色点表示
			drawBox(frame, pbox);
			detections++;
		}
		//Display
		t = ((double)getTickCount() - t) / getTickFrequency();
		fps = 1.0 / t;
		sprintf(str, "%5f", fps);
		string fpsStr("FPS:");
		fpsStr += str;
		putText(frame, fpsStr, Point(0, 20), FONT_HERSHEY_DUPLEX, 0.7, Scalar(0, 250, 250));
		imshow("TLD", frame);
		WriteChar(target, 9);
		//swap points and images
		swap(last_gray, current_gray);  //STL函数swap()用来交换两对象的值。其泛型化版本定义于<algorithm>;
		pts1.clear();
		pts2.clear();
		frames++;
		printf("Detection rate: %d/%d\n", detections, frames);
		cap >> frame;
		if (cvWaitKey(5) == 'q')
			break;
	}
	if (rep)
	{
		rep = false;
		tl = false;
		fclose(bb_file);
		bb_file = fopen("final_detector.txt", "w");
		//capture.set(CV_CAP_PROP_POS_AVI_RATIO,0);
		//cap.release();
		//cap.open(video);
		goto REPEAT;
	}
	fclose(bb_file);
	return 0;
}




