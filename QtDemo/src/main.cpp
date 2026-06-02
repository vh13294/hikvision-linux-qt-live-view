/*
 * Copyright(C) 2010,Hikvision Digital Technology Co., Ltd 
 * 
 * �ļ����ƣ�main.cpp
 * ��    ����
 * ��ǰ�汾��1.0
 * ��    �ߣ�������
 * �������ڣ�2009��11��12��
 * �޸ļ�¼��
 */

#include "qtclientdemo.h"
#include <QTextCodec>
#include <QtGui>
#include <QApplication>


//������
QtClientDemo* gqtclinetdemo;

/*******************************************************************
      Function:   main
   Description:   ���������
     Parameter:   (IN)   int argc  
                  (IN)   char *argv[]  
        Return:   0--�ɹ���-1--ʧ�ܡ�   
**********************************************************************/
int main(int argc, char *argv[])
{
	gqtclinetdemo = NULL;   
    QApplication a(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("GB2312"));

    //Add translation file.
    QTranslator translator;
    BOOL bSuc;
    bSuc = translator.load("..//..//..//..//translation//QtDemo_zh_CN");
    if (!bSuc)
    {
         bSuc = translator.load("..//..//translation//QtDemo_zh_CN");
    }
    if (!bSuc)
    {
        translator.load("translation/QtDemo_zh_CN");
    }
    if (!bSuc)
    {
        translator.load("./QtDemo_zh_CN");
    }
    a.installTranslator(&translator);

    //Show the main window.
    QtClientDemo w;
	w.show();
	gqtclinetdemo =&w;
    return a.exec();
}
