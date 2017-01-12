// FTRTest.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "windows.h"
#include <fstream>
#include <string>
#include <iostream>
#include <unordered_map>
#include <locale.h>
#include <direct.h>
#include <time.h>

using namespace std;

#define MAX_RET_NUM 4096      // max number of result to be returned
#define MAX_CI_NUM 1001		//长度足够大，或者比已知长度词表多一个
#define MAX_CORPUS_COUNT 2048
#define MAX_ZI_NUM_IN_A_CI 5
//#define _CRT_SECURE_NO_WARNINGS

struct CiFrequency
{
	wchar_t * ci;
	int cp;
	CiFrequency(){
		ci=NULL;
		cp=0;
	}
};

struct IdxNode
{
	__int64 DID;
	__int64 POS;
	int LHD;
	__int64 JuID;
	IdxNode(){
		DID=0;
		POS=0;
		LHD=0;
		JuID=0;
	}
};

struct Ci2IdxNode
{

	wchar_t * ci;
	__int64 nStart;
	__int64 nEnd;
	Ci2IdxNode(){
		ci=NULL;
		nStart=-1;
		nEnd=-1;
	}
};

struct BFLhd
{
	int nForwardLHD;
	int nBackLHD;
	__int64 nPos;
	__int64 nJuID;
	BFLhd(){
		nForwardLHD=0;
		nBackLHD=0;
		nPos=0;
		nJuID=0;
	}
};

enum FTRTYPE
{
	FTRTYPE_BACK,
	FTRTYPE_FORWARD
};

/////////////////////////
#define CMP_MAXLEN 5

__int64 g_nCiCount = 0;
__int64 g_nJuCount = 0;
__int64 g_nTotalCharNum = 0;	//total character count of segmented & scanned output file (with space)
__int64 g_nTotalTextNum = 0;	//total character count of text & scanned output file (without space)
__int64 g_nTotalCiNum = 0;

const wchar_t* g_pBuffer;

wchar_t * g_ftrDat = NULL;
Ci2IdxNode* g_ftrCi2Idx = NULL;
IdxNode* g_ftrBackIdx = NULL;
IdxNode* g_ftrForwardIdx = NULL;

int Compare( const void *arg1, const void *arg2 )
{
	CiFrequency *pContent1 = (CiFrequency *)arg1;
	CiFrequency *pContent2 = (CiFrequency *)arg2;

	int nRet = wcscmp(pContent1->ci, pContent2->ci);
	//int nRet = wcsncmp(pContent1->ci, pContent2->ci, CMP_MAXLEN);

	return nRet;
}

int IdxBackCompare( const void *arg1, const void *arg2 )
{
	IdxNode *pContent1 = (IdxNode *)arg1;
	IdxNode *pContent2 = (IdxNode *)arg2;

	int nRet = wcsncmp(&g_pBuffer[pContent1->POS + pContent1->LHD], &g_pBuffer[pContent2->POS + pContent2->LHD], CMP_MAXLEN);
	return nRet;
}

int IdxForwardCompare( const void *arg1, const void *arg2 )
{
	IdxNode *pContent1 = (IdxNode *)arg1;
	IdxNode *pContent2 = (IdxNode *)arg2;

	__int64 nStartPos1 = pContent1->POS - pContent1->LHD;
	__int64 nStartPos2 = pContent2->POS - pContent2->LHD;

	wchar_t str1[CMP_MAXLEN + 1] = L"", str2[CMP_MAXLEN + 1] = L"";

	for (__int64 i = 0; i < CMP_MAXLEN; i++){
		if((nStartPos1-i)>=0)
			str1[i] = g_pBuffer[nStartPos1-i];
		else
			str1[i] = '\0';
	}
	for (__int64 i = 0; i < CMP_MAXLEN; i++){
		if((nStartPos2-i)>=0)
			str2[i] = g_pBuffer[nStartPos2-i];
		else
			str2[i] = '\0';
	}
	str1[CMP_MAXLEN] = '\0';str2[CMP_MAXLEN] = '\0';
	int nRet = wcsncmp(str1, str2, CMP_MAXLEN);

	return nRet;
}

int FTRCompare(const void *arg1, const void *arg2 )
{
	Ci2IdxNode *pContent1 = (Ci2IdxNode *)arg1;
	Ci2IdxNode *pContent2 = (Ci2IdxNode *)arg2;

	int nRet = wcscmp(pContent1->ci, pContent2->ci);
	return nRet;
}

bool ReadDict(CiFrequency *& pnCF, wchar_t* fnDict){
	unsigned short Word;
	wchar_t tempCi[8];
	int tempCiLength = 0;
	FILE* fpDict = NULL;
	fpDict = _wfopen(fnDict, L"rb");
	if( fpDict == NULL )
		return false;

	fseek(fpDict,0,SEEK_END);
	__int64 nSize=ftell(fpDict);
	rewind(fpDict);
	char* psDictBuff = new char[nSize];
	fread(psDictBuff, sizeof(char), nSize, fpDict);
	fclose(fpDict);

	for(__int64 i=0;i<nSize;i++){
		int nLen=1;
		if((psDictBuff[i] >= '0')&&(psDictBuff[i] <= '9')||(psDictBuff[i] == '\t')){
			continue;
		}
		if( (psDictBuff[i] == '\r')&&(psDictBuff[i+1] == '\n') ){
			tempCi[tempCiLength] = L'\0';

			pnCF[g_nCiCount].ci = new wchar_t[tempCiLength + 1];
			pnCF[g_nCiCount].cp = 0;

			wcscpy_s(pnCF[g_nCiCount].ci, tempCiLength + 1, (const wchar_t *)tempCi);

			i++;g_nCiCount++;tempCiLength = 0;
			continue;
		}

		if ( (unsigned char)psDictBuff[i] > 0x80 ){
			nLen=2;
		}

		MultiByteToWideChar(CP_ACP,0,&psDictBuff[i],nLen,(LPWSTR)&Word, 1);
		tempCi[tempCiLength] = Word;	tempCiLength++;

		if( nLen == 2 )
			i++;
	}
	qsort(pnCF, g_nCiCount, sizeof(CiFrequency), Compare);

	delete[] psDictBuff;
	return true;
}

bool findCiPtrByUnicode(CiFrequency *& pnWC, CiFrequency * tarCi, CiFrequency *& resCi){
	resCi = (CiFrequency*)bsearch(tarCi, pnWC, g_nCiCount, sizeof(CiFrequency), Compare);

	if (resCi==NULL){
		return 0;
	}
	return 1;
}
bool IsIn(wchar_t target, const wchar_t * beSearchedArray = L" 　\r\n\t/abcdefghijgklmnopqrstuvwxyzABCDEFGHIGKLMNOPQRSTUVWXYZ"){
	for (int i = 0; i < wcslen(beSearchedArray); i++){
		if (target == beSearchedArray[i]){
			return true;
		}
	}
	return false;

}
void WriteDataNew(char* psBuff,__int64 nSize,FILE* fpSegOut,FILE* fpTextOut,__int64& nCiNum,CiFrequency*& pnWC)
{
	nCiNum=0;
	unsigned short ptrOriginPosWord = 0;
	CiFrequency curCiFq;
	curCiFq.ci = new wchar_t[16];
	curCiFq.cp = 0;

	CiFrequency * searchResCiFq = NULL;

	int CurCiLength = 0, nLen=1;
	printf("\nTotal %d chars.\n", nSize);

	for(__int64 i=0;i<nSize;i++){
		printf("\r%d-th char is processing...", i);
		if (IsIn(psBuff[i])){
			continue;
		}else{
			CurCiLength = 0;
			do{
				nLen = 1;
				if ( (unsigned char)psBuff[i] > 0x80 ){
					MultiByteToWideChar(CP_ACP,0,&psBuff[i],++nLen,(LPWSTR)&ptrOriginPosWord, 1);
				}else{
					MultiByteToWideChar(CP_ACP,0,&psBuff[i],nLen,(LPWSTR)&ptrOriginPosWord, 1);
				}
				i += nLen;
				if (ptrOriginPosWord == 0x3000){
					continue;
				}
				curCiFq.ci[CurCiLength] = ptrOriginPosWord;
				fwrite(&ptrOriginPosWord, sizeof(unsigned short), 1, fpSegOut);
				fwrite(&ptrOriginPosWord, sizeof(unsigned short), 1, fpTextOut);
				g_nTotalCharNum++;
				g_nTotalTextNum++;
				CurCiLength++;
			} while (!IsIn(psBuff[i]));
			i--;
			if (CurCiLength){
				curCiFq.ci[CurCiLength] = '\0';
				//printf("\r%d-th Ci is processing...", g_nCiCount);
				fwrite(L" ", sizeof(unsigned short), 1, fpSegOut);
				g_nTotalCharNum++;

				if(findCiPtrByUnicode(pnWC, &curCiFq, searchResCiFq)){
					//判断语料库中的每个词是否在词表里
					nCiNum++;
					searchResCiFq->cp++;
				}else{
					/*if (g_nCiCount < MAX_CI_NUM){
					pnWC[g_nCiCount].ci = new wchar_t[CurCiLength + 1];
					pnWC[g_nCiCount].cp++;
					wcscpy_s(pnWC[g_nCiCount].ci, CurCiLength + 1, (const wchar_t *)curCiFq.ci);
					g_nCiCount++;

					qsort(pnWC, g_nCiCount, sizeof(CiFrequency), Compare);
					}*/
				}
			}
		}
	}

	if (curCiFq.ci != NULL){delete[] curCiFq.ci;  curCiFq.ci = NULL;}
	if (searchResCiFq != NULL){searchResCiFq = NULL;}
}

void WriteData(char* psBuff,__int64 nSize,FILE* fpOut,__int64& nCiNum,CiFrequency*& pnWC)
{
	nCiNum=0;
	unsigned short Word;
	CiFrequency curCiFq;
	curCiFq.ci = new wchar_t[8];
	curCiFq.cp = 0;

	CiFrequency * searchResCiFq = NULL;

	int CurCiLength = 0;
	for(__int64 i=0;i<nSize;i++){
		int nLen=1;
		if(psBuff[i] == 0x20){
			curCiFq.ci[CurCiLength] = '\0';
			CurCiLength++;

			if(findCiPtrByUnicode(pnWC, &curCiFq, searchResCiFq)){
				searchResCiFq->cp++;
			}else{
				/*
				pnWC[g_nCiCount].ci = new wchar_t[CurCiLength + 1];
				pnWC[g_nCiCount].cp++;
				wcscpy_s(pnWC[g_nCiCount].ci, CurCiLength + 1, (const wchar_t *)curCiFq.ci);
				g_nCiCount++;
				qsort(pnWC, g_nCiCount, sizeof(CiFrequency), Compare);
				*/
			}

			CurCiLength = 0;
			nCiNum++;

			MultiByteToWideChar(CP_ACP,0,&psBuff[i],nLen,(LPWSTR)&Word, 1);
			fwrite(&Word,sizeof(unsigned short),1,fpOut);
			g_nTotalCharNum++;
			continue;
		}

		if ( (unsigned char)psBuff[i] > 0x80 ){
			nLen=2;
		}

		MultiByteToWideChar(CP_ACP,0,&psBuff[i],nLen,(LPWSTR)&Word, 1);
		curCiFq.ci[CurCiLength] = Word;	CurCiLength++;

		if( nLen == 2 )
			i++;

		fwrite(&Word,sizeof(unsigned short),1,fpOut);
		g_nTotalCharNum++;
	}

	if (curCiFq.ci != NULL){delete[] curCiFq.ci; curCiFq.ci = NULL;}
	if (searchResCiFq != NULL){searchResCiFq = NULL;}
}

bool MergeFilesUnicode(wchar_t* psFiles[],int nFileNum,char* psSegData, char* psTextData, CiFrequency*& pnWC,__int64& nTotalCiNum)
{
	nTotalCiNum=0;
	FILE* fpSegOut = NULL;
	fpSegOut = fopen(psSegData,"wb");
	if( fpSegOut == NULL )
		return false;

	FILE* fpTextgOut = NULL;
	fpTextgOut = fopen(psTextData,"wb");
	if( fpTextgOut == NULL )
		return false;

	printf("\n");
	for(int i=0;i<nFileNum;i++){
		FILE* fpInp = NULL;
		printf("\r%d-th file is processing.........", i);
		fpInp=_wfopen(psFiles[i], L"rb");
		if( fpInp == NULL )
			continue;
		fseek(fpInp,0,SEEK_END);
		__int64 nSize=ftell(fpInp);
		rewind(fpInp);
		char* psBuff=new char[nSize];
		fread(psBuff,sizeof(char),nSize,fpInp);
		fclose(fpInp);

		__int64 nCiNum = 0;
		WriteDataNew(psBuff, nSize, fpSegOut, fpTextgOut, nCiNum, pnWC);
		//WriteData(psBuff,nSize,fpOut,nCiNum,pnWC);
		nTotalCiNum += nCiNum;

		delete[] psBuff;
	}
	//fwrite(&Word,sizeof(unsigned short),1,fpOut);

	fclose(fpSegOut);
	fclose(fpTextgOut);
	return true;
}

void GetReadyCi(CiFrequency* pnWC)
{
	pnWC[0].cp = pnWC[0].cp * CMP_MAXLEN - 1;

	for(__int64 i=1; i <= g_nCiCount; i++){
		pnWC[i].cp = pnWC[i-1].cp + pnWC[i].cp * CMP_MAXLEN;
	}
}

void RecoveryCi(CiFrequency* pnWC)
{
	for(__int64 i=0; i <= g_nCiCount - 1; i++){
		pnWC[i].cp = (pnWC[i+1].cp - pnWC[i].cp) / CMP_MAXLEN;
	}
	pnWC[g_nCiCount].cp = 0;
}

void SortLHBlock(int nStart,int nEnd,IdxNode* pnPOS,wchar_t* psBuff, int (*compare)(const void *, const void *) )
{
	nStart++; nEnd++;
	g_pBuffer=psBuff;
	qsort(&pnPOS[nStart],nEnd-nStart,sizeof(IdxNode),compare);
}

void SortLHIdx(CiFrequency* pnWC, IdxNode* pnPOS, wchar_t* psBuff, int (*compare)(const void *, const void *) )
{
	for(__int64 i=0;i < g_nCiCount - 1;i++){
		if(pnWC[i].cp != pnWC[i+1].cp){
			SortLHBlock(pnWC[i].cp,pnWC[i+1].cp, pnPOS, psBuff, compare);
		}
	}
}

bool CreateIdxDat(char* psSegFile, char* psTextFile, CiFrequency*& pnWC, IdxNode*& pnPOS, FTRTYPE FtrType)
{
	GetReadyCi(pnWC);

	//read orgin corpus
	FILE* fpInp;
	fpInp = fopen(psSegFile,"rb");
	if( fpInp == NULL )
		return false;

	wchar_t* psBuff=new wchar_t[g_nTotalCharNum+1];
	fread(psBuff,sizeof(wchar_t),g_nTotalCharNum, fpInp);
	fclose(fpInp);
	psBuff[g_nTotalCharNum] = L'\0';

	__int64 PreviousUniSpacePos = 0;
	__int64 SpaceCount = 0;

	//current ci
	CiFrequency curCiFq;
	curCiFq.ci = new wchar_t[16];
	curCiFq.cp = 0;

	CiFrequency * searchResCiFq = NULL;

	//start scan every char in file
	for( __int64 i=0; i < g_nTotalCharNum; i++){
		if (psBuff[i] == L'，'|| psBuff[i] == L'。'|| psBuff[i] == L'！'|| psBuff[i] == L'；'|| psBuff[i] == L'？'){
			g_nJuCount ++;
		}
		if(psBuff[i] == 0x20){
			SpaceCount++;
			//get current ci OR last ci
			__int64 curCiLen = i-PreviousUniSpacePos+1;

			for (__int64 k = i-PreviousUniSpacePos; k > 0; k--){
				curCiFq.ci[i-PreviousUniSpacePos-k] = psBuff[i-k];
			}
			curCiFq.ci[i-PreviousUniSpacePos] = L'\0';

			//find ci & construct idx node & ci_idx++
			if(findCiPtrByUnicode(pnWC, &curCiFq, searchResCiFq)){
				for (__int64 loop = 0; loop < CMP_MAXLEN; loop++){
					pnPOS[searchResCiFq->cp].DID = 0;
					pnPOS[searchResCiFq->cp].POS = i - SpaceCount;
					if (FtrType == FTRTYPE_FORWARD){
						pnPOS[searchResCiFq->cp].LHD = (i - loop - curCiLen < 0 ? 0 : loop + curCiLen);			//0 isn't avalible?
					}else if (FtrType == FTRTYPE_BACK){
						pnPOS[searchResCiFq->cp].LHD = (loop + i >= g_nTotalCharNum ? 0 : loop);			//0 isn't avalible?
					}else{
						pnPOS[searchResCiFq->cp].LHD = 0;			//0 isn't avalible?
					}
					pnPOS[searchResCiFq->cp].JuID = g_nJuCount;
					searchResCiFq->cp--;
				}
			}

			PreviousUniSpacePos = i + 1;
		}
	}

	if (psBuff != NULL){delete[] psBuff;}		//delete psBuff just-in-time for less use of computer memory

	fpInp = fopen(psTextFile,"rb");
	if( fpInp == NULL )
		return false;
	psBuff = new wchar_t[g_nTotalTextNum+1];
	fread(psBuff,sizeof(wchar_t),g_nTotalTextNum, fpInp);
	psBuff[g_nTotalTextNum] = L'\0';
	fclose(fpInp);

	if (FtrType == FTRTYPE_FORWARD){
		SortLHIdx(pnWC, pnPOS, psBuff, IdxForwardCompare);
	}else if (FtrType == FTRTYPE_BACK){
		SortLHIdx(pnWC, pnPOS, psBuff, IdxBackCompare);
	}else{
		SortLHIdx(pnWC, pnPOS, psBuff, IdxBackCompare);
	}

	RecoveryCi(pnWC);

	if (curCiFq.ci != NULL){delete curCiFq.ci;}

	if (searchResCiFq != NULL){searchResCiFq = NULL;}
	return true;
}

bool WriteCi2Idx(char* psWC,CiFrequency* pnWC)
{
	FILE* fpOut = NULL;
	fpOut=fopen(psWC,"wb");
	if( fpOut == NULL )
		return false;

	fwrite(&g_nCiCount, sizeof(__int64), 1, fpOut);
	fwrite(&g_nTotalCharNum, sizeof(__int64), 1, fpOut);
	fwrite(&g_nTotalTextNum, sizeof(__int64), 1, fpOut);

	__int64 curStartPos = 0;
	for (__int64 i = 0; i < g_nCiCount; i++)
	{
		int curCilen = wcslen(pnWC[i].ci);
		fwrite(&curStartPos,sizeof(__int64), 1, fpOut);
		fwrite(&pnWC[i].cp,sizeof(int), 1, fpOut);
		fwrite(&curCilen, sizeof(int), 1, fpOut);
		fwrite(pnWC[i].ci,sizeof(wchar_t), curCilen, fpOut);

		curStartPos += pnWC[i].cp;
		//fwrite(&L"\n",sizeof(wchar_t), 1, fpOut);
	}
	//fwrite(pnWC,sizeof(int),0x10000,fpOut);
	fclose(fpOut);
	return true;
}

bool WriteLHIdx(IdxNode* pnPOS,__int64 nTotalCiNum,char* psIdxDat)
{
	FILE* fpOut = NULL;
	fpOut=fopen(psIdxDat,"wb");
	if( fpOut == NULL )
		return false;

	fwrite(&nTotalCiNum,sizeof(__int64),1,fpOut);

	for (__int64 i = 0; i < nTotalCiNum * CMP_MAXLEN; i++)
	{
		fwrite(&pnPOS[i].DID, sizeof(__int64), 1, fpOut);
		fwrite(&pnPOS[i].POS, sizeof(__int64), 1, fpOut);
		fwrite(&pnPOS[i].LHD, sizeof(int), 1, fpOut);
		fwrite(&pnPOS[i].JuID, sizeof(__int64), 1, fpOut);
	}

	fclose(fpOut);
	return true;
}

bool CreateIdx(wchar_t* psDict, wchar_t* psFiles[],int nFileNum,char* psSegData, char* psTextData,char* psHZ,char* psBackIdxDat, char* psForwardIdxDat)
{
	struct CiFrequency * pnWC = NULL;
	struct IdxNode * pnCPos = NULL;

	__int64 nTotalCharNum = 0;
	__int64 nTotalCiNum;

	pnWC = new CiFrequency[MAX_CI_NUM];

	if ( !ReadDict(pnWC, psDict)  ){
		return false;
	}

	if ( !MergeFilesUnicode(psFiles, nFileNum, psSegData, psTextData, pnWC, nTotalCiNum)  ){
		return false;
	}

	//Create BackIdx
	pnCPos = new IdxNode[nTotalCiNum * CMP_MAXLEN];
	if ( !CreateIdxDat(psSegData, psTextData, pnWC, pnCPos, FTRTYPE_BACK) ){
		return false;
	}
	if ( !WriteLHIdx(pnCPos, nTotalCiNum, psBackIdxDat) ){
		return false;
	}
	if (pnCPos != NULL){delete[] pnCPos; pnCPos = NULL;}

	g_nJuCount = 0;

	//Create ForwardIdx
	pnCPos = new IdxNode[nTotalCiNum * CMP_MAXLEN];
	if ( !CreateIdxDat(psSegData, psTextData, pnWC, pnCPos, FTRTYPE_FORWARD) ){
		return false;
	}
	if ( !WriteLHIdx(pnCPos, nTotalCiNum, psForwardIdxDat) ){
		return false;
	}
	if (pnCPos != NULL){delete[] pnCPos; pnCPos = NULL;}

	if ( !WriteCi2Idx(psHZ, pnWC) ){
		return false;
	}

	for (__int64 i = 0; i < g_nCiCount; i++)
	{
		if ( pnWC[i].ci != NULL){
			delete[] pnWC[i].ci;
		}
	}
	if (pnWC != NULL){delete[] pnWC;}
	return true;

}

bool FTRLHInit(char* psWC,char* psDat,char* psBackIdx, char* psForwardIdx)
{
	//////////////////////////////////////////////////
	//READ FIILES OF CI2IDX
	FILE* fpInp=fopen(psWC,"rb");
	if( fpInp == NULL )
		goto GOTO_ERROR;

	fread(&g_nCiCount, sizeof(__int64), 1, fpInp);
	fread(&g_nTotalCharNum, sizeof(__int64), 1, fpInp);
	fread(&g_nTotalTextNum, sizeof(__int64), 1, fpInp);

	g_ftrCi2Idx = new Ci2IdxNode[g_nCiCount];
	for (__int64 i = 0; i < g_nCiCount; i++)
	{
		int curCiLen = 0, curCiCount = 0;
		__int64 curStartPos = 0;

		fread(&curStartPos, sizeof(__int64), 1, fpInp);
		fread(&curCiCount, sizeof(int), 1, fpInp);

		g_ftrCi2Idx[i].nStart = curStartPos * CMP_MAXLEN;
		g_ftrCi2Idx[i].nEnd = ( curStartPos + curCiCount ) * CMP_MAXLEN;

		fread(&curCiLen, sizeof(int), 1, fpInp);

		g_ftrCi2Idx[i].ci = new wchar_t[curCiLen + 1];
		fread(g_ftrCi2Idx[i].ci, sizeof(wchar_t), curCiLen, fpInp);
		g_ftrCi2Idx[i].ci[curCiLen] = '\0';
	}
	fclose(fpInp);

	//////////////////////////////////////////////////
	//READ FIILES OF BACK IDX
	fpInp = fopen(psBackIdx,"rb");
	if( fpInp == NULL )
		goto GOTO_ERROR;

	fread(&g_nTotalCiNum, sizeof(__int64), 1, fpInp);
	g_ftrBackIdx = new IdxNode[g_nTotalCiNum * CMP_MAXLEN];
	for (__int64 i = 0; i < g_nTotalCiNum * CMP_MAXLEN; i++)
	{
		fread(&g_ftrBackIdx[i].DID, sizeof(__int64), 1, fpInp);
		fread(&g_ftrBackIdx[i].POS, sizeof(__int64), 1, fpInp);
		fread(&g_ftrBackIdx[i].LHD, sizeof(int), 1, fpInp);
		fread(&g_ftrBackIdx[i].JuID, sizeof(__int64), 1, fpInp);
	}
	fclose(fpInp);

	//////////////////////////////////////////////////
	//READ FIILES OF FORWARD IDX
	fpInp = fopen(psForwardIdx,"rb");
	if( fpInp == NULL )
		goto GOTO_ERROR;

	fread(&g_nTotalCiNum, sizeof(__int64), 1, fpInp);
	g_ftrForwardIdx = new IdxNode[g_nTotalCiNum * CMP_MAXLEN];
	for (__int64 i = 0; i < g_nTotalCiNum * CMP_MAXLEN; i++)
	{
		fread(&g_ftrForwardIdx[i].DID, sizeof(__int64), 1, fpInp);
		fread(&g_ftrForwardIdx[i].POS, sizeof(__int64), 1, fpInp);
		fread(&g_ftrForwardIdx[i].LHD, sizeof(int), 1, fpInp);
		fread(&g_ftrForwardIdx[i].JuID, sizeof(__int64), 1, fpInp);
	}
	fclose(fpInp);

	//////////////////////////////////////////////////
	//READ FIILES OF ORIGINAL FILES
	fpInp=fopen(psDat,"rb");
	if( fpInp == NULL )
		goto GOTO_ERROR;

	fseek(fpInp,0,SEEK_END);
	__int64 nSize = ftell(fpInp);
	rewind(fpInp);
	g_ftrDat = new wchar_t[nSize];
	fread(g_ftrDat, sizeof(wchar_t), nSize, fpInp);
	fclose(fpInp);

	return true;

GOTO_ERROR:
	for (__int64 i = 0; i < g_nCiCount; i++){
		delete[] g_ftrCi2Idx[i].ci;
	}
	if ( g_ftrCi2Idx != NULL  )delete[] g_ftrCi2Idx;
	if ( g_ftrBackIdx != NULL  )delete g_ftrBackIdx;
	if ( g_ftrForwardIdx != NULL  )delete g_ftrForwardIdx;
	if ( g_ftrDat != NULL  )delete g_ftrDat;
	g_ftrCi2Idx=NULL;
	//此处还需要delete g_ftrCi2Idx 中的ci

	g_ftrBackIdx=NULL;
	g_ftrForwardIdx=NULL;
	g_ftrDat=NULL;

	return false;
}

void FTRLHExit()
{
	for (__int64 i = 0; i < g_nCiCount; i++){
		if (g_ftrCi2Idx[i].ci != NULL){delete[] g_ftrCi2Idx[i].ci;}
	}
	if ( g_ftrCi2Idx != NULL  )delete[] g_ftrCi2Idx;
	if ( g_ftrBackIdx != NULL  )delete[] g_ftrBackIdx;
	if ( g_ftrForwardIdx != NULL  )delete[] g_ftrForwardIdx;
	if ( g_ftrDat != NULL  )delete[] g_ftrDat;
	g_ftrCi2Idx=NULL;
	g_ftrBackIdx=NULL;
	g_ftrForwardIdx=NULL;
	g_ftrDat=NULL;
}

__int64 bBackLHSearchLow(__int64 nStart, __int64 nEnd , wchar_t *psQuery)
{
	int  CompRes = 0;
	__int64 mid = 0, Ret= 0;

	wchar_t cmpCi[CMP_MAXLEN + 1];
	if( nStart >= nEnd )
		return -1;

	int nLen = wcslen((const wchar_t *)psQuery);

	if ( nLen > CMP_MAXLEN )
		nLen = CMP_MAXLEN;

	wcsncpy_s(cmpCi, &g_ftrDat[g_ftrBackIdx[nStart].POS + g_ftrBackIdx[nStart].LHD], nLen);

	CompRes = wcsncmp(cmpCi, psQuery, nLen);

	if( CompRes == 0 )
		return nStart;

	if( nEnd - nStart == 1)
		return -1;

	if( CompRes > 0 )
		return -1;

	mid = __int64((nStart + nEnd)/2);

	Ret = bBackLHSearchLow( nStart , mid , psQuery);
	if (Ret == -1 )
		return bBackLHSearchLow( mid, nEnd , psQuery);
	return Ret;
}


__int64 bBackLHSearchHigh(__int64 nStart, __int64 nEnd ,wchar_t *psQuery)
{
	int  CompRes = 0;
	__int64 mid = 0, Ret= 0;

	wchar_t cmpCi[CMP_MAXLEN + 1];

	if( nStart >= nEnd )
		return -1;

	int nLen = wcslen((const wchar_t *)psQuery);

	if ( nLen > CMP_MAXLEN )
		nLen = CMP_MAXLEN;

	wcsncpy_s(cmpCi, &g_ftrDat[g_ftrBackIdx[nEnd-1].POS + g_ftrBackIdx[nEnd-1].LHD], nLen);

	CompRes = wcsncmp(cmpCi, psQuery, nLen);

	if( CompRes == 0 )
		return nEnd;

	if( nEnd - nStart == 1 )
		return -1;

	if( CompRes < 0 )
		return -1;

	mid = __int64(( nStart + nEnd)/2);
	Ret = bBackLHSearchHigh( mid , nEnd , psQuery);
	if ( Ret == -1 )
		return bBackLHSearchHigh( nStart, mid , psQuery);
	return Ret;
}


__int64 bForwardLHSearchLow(__int64 nStart, __int64 nEnd , wchar_t *psQuery)
{
	int  CompRes = 0;
	__int64 mid = 0, Ret= 0;

	wchar_t cmpCi[CMP_MAXLEN + 1];

	if( nStart >= nEnd )
		return -1;

	int nLen = wcslen((const wchar_t *)psQuery);

	if ( nLen > CMP_MAXLEN )
		nLen = CMP_MAXLEN;

	for (__int64 j = 0, i = g_ftrForwardIdx[nStart].POS - g_ftrForwardIdx[nStart].LHD; i > g_ftrForwardIdx[nStart].POS - g_ftrForwardIdx[nStart].LHD - nLen; i--,j++){
		cmpCi[j] = g_ftrDat[i];
	}
	cmpCi[nLen] = L'\0';

	CompRes = wcsncmp(cmpCi, psQuery, nLen);
	//CompRes = wcsncmp(&g_ftrDat[g_ftrForwardIdx[nStart].POS - g_ftrForwardIdx[nStart].LHD], psQuery, 1);

	if( CompRes == 0 )
		return nStart;

	if( nEnd - nStart == 1)
		return -1;

	if( CompRes > 0 )
		return -1;

	mid = __int64((nStart + nEnd)/2);

	Ret = bForwardLHSearchLow( nStart , mid , psQuery);
	if (Ret == -1 )
		return bForwardLHSearchLow( mid, nEnd , psQuery);
	return Ret;
}


__int64 bForwardLHSearchHigh(__int64 nStart, __int64 nEnd ,wchar_t *psQuery)
{
	int  CompRes = 0;
	__int64 mid = 0, Ret= 0;

	wchar_t cmpCi[CMP_MAXLEN + 1];
	if( nStart >= nEnd )
		return -1;

	int nLen = wcslen((const wchar_t *)psQuery);

	if ( nLen > CMP_MAXLEN )
		nLen = CMP_MAXLEN;

	for (__int64 j = 0, i = g_ftrForwardIdx[nEnd-1].POS - g_ftrForwardIdx[nEnd-1].LHD; i > g_ftrForwardIdx[nEnd-1].POS - g_ftrForwardIdx[nEnd-1].LHD - nLen; i--,j++){
		cmpCi[j] = g_ftrDat[i];
	}
	cmpCi[nLen] = L'\0';

	CompRes = wcsncmp(cmpCi, psQuery, nLen);
	//CompRes = wcsncmp(&g_ftrDat[g_ftrForwardIdx[nEnd-1].POS - g_ftrForwardIdx[nEnd-1].LHD], psQuery, 1);

	if( CompRes == 0 )
		return nEnd;

	if( nEnd - nStart == 1 )
		return -1;

	if( CompRes < 0 )
		return -1;

	mid = __int64(( nStart + nEnd)/2);
	Ret = bForwardLHSearchHigh( mid , nEnd , psQuery);
	if ( Ret == -1 )
		return bForwardLHSearchHigh( nStart, mid , psQuery);
	return Ret;
}

bool SearchBackLHFTR(int psInp1Len, wchar_t* psInp2, __int64 nCi2IdxStart, __int64 nCi2IdxEnd, __int64 & nRetStart, __int64 & nRetEnd, __int64 & Num, int nMaxRetLen = -1){
	int nLen = wcslen(psInp2);

	nRetStart = bBackLHSearchLow(nCi2IdxStart, nCi2IdxEnd, psInp2);
	if ( nRetStart == -1 ){
		Num=0;
		return false;
	}

	nRetEnd = bBackLHSearchHigh(nCi2IdxStart, nCi2IdxEnd, psInp2);

	Num = nRetEnd - nRetStart;
	if (( Num > nMaxRetLen) && (nMaxRetLen != -1)){
		Num=nMaxRetLen;
	}
	return true;
}
bool SearchForwardLHFTR(int psInp1Len, wchar_t* psInp2, __int64 nCi2IdxStart, __int64 nCi2IdxEnd, __int64 & nRetStart, __int64 & nRetEnd, __int64 & Num, int nMaxRetLen = -1){
	int nLen = wcslen(psInp2);

	wchar_t * psInp2Reverse = new wchar_t[nLen + 1];
	for (int i = 0; i < nLen; i++){
		psInp2Reverse[i] = psInp2[nLen-i-1];
	}
	psInp2Reverse[nLen] = '\0';

	nRetStart=bForwardLHSearchLow(nCi2IdxStart, nCi2IdxEnd, psInp2Reverse);
	if ( nRetStart == -1 ){
		Num=0;
		return false;
	}

	nRetEnd=bForwardLHSearchHigh(nCi2IdxStart, nCi2IdxEnd, psInp2Reverse);

	Num = nRetEnd - nRetStart;
	if (( Num > nMaxRetLen) && (nMaxRetLen != -1)){
		Num=nMaxRetLen;
	}
	return true;
}
bool FTRLHCore(wchar_t* psInp1,wchar_t* psInp2,char** psRet,int nMaxRetLen,__int64 & Num, __int64 & nRetStart, __int64 & nRetEnd, FTRTYPE FtrType)
{
	/*
	nRetStart: Start position of search result in Idx
	nRetEnd: End position of search result in Idx
	*/
	int nLen = wcslen(psInp1);

	if ( nLen == 0 ){
		Num=0;
		return false;
	}

	Ci2IdxNode *new_ci = new Ci2IdxNode();
	new_ci->ci = psInp1;
	new_ci->nEnd = new_ci->nStart = 0;

	Ci2IdxNode *searchResOfCi2Idx = (Ci2IdxNode*)bsearch(new_ci, g_ftrCi2Idx, g_nCiCount, sizeof(Ci2IdxNode), FTRCompare);
	if (searchResOfCi2Idx == NULL)
		return false;

	delete new_ci;

	if (FtrType == FTRTYPE_FORWARD)
	{
		return SearchForwardLHFTR(nLen, psInp2, searchResOfCi2Idx->nStart, searchResOfCi2Idx->nEnd, nRetStart, nRetEnd, Num, nMaxRetLen);
	}else if (FtrType == FTRTYPE_BACK){
		return SearchBackLHFTR(nLen, psInp2, searchResOfCi2Idx->nStart, searchResOfCi2Idx->nEnd, nRetStart, nRetEnd, Num, nMaxRetLen);
	}else{
		return SearchBackLHFTR(nLen, psInp2, searchResOfCi2Idx->nStart, searchResOfCi2Idx->nEnd, nRetStart, nRetEnd, Num, nMaxRetLen);
	}

}

void InpParser(wchar_t * Inp, int & FtrArgsNum, wchar_t *& KeyWord, wchar_t *& FtrBackArgs, wchar_t *& FtrForwardArgs){
	/*
	FtrArgsNum：
	0: error
	1: only back;
	2: only forward;
	3: back & forward;
	4: only ftr no LH;
	*/
	wchar_t seps[]= L"*", *token = NULL, *segmentRes[3];
	short InpCiCount = 1;

	segmentRes[0] = token = wcstok( Inp, seps );
	for (int i = 1; (token != NULL)&&(i<3); i++)
	{
		segmentRes[i] = token = wcstok( NULL, seps );
		if (token){
			InpCiCount++;
		}
	}

	Ci2IdxNode *searchRes = NULL,*new_ci = NULL;
	new_ci = new Ci2IdxNode();

	switch (InpCiCount)
	{
	case 3:
		new_ci->ci = segmentRes[1];
		new_ci->nEnd = new_ci->nStart = 0;

		searchRes = (Ci2IdxNode*)bsearch(new_ci, g_ftrCi2Idx, g_nCiCount, sizeof(Ci2IdxNode), FTRCompare);
		if (searchRes){
			FtrArgsNum = 3;
			//KeyWord = new wchar_t[wcslen(segmentRes[1])];
			//FtrBackArgs = new wchar_t[wcslen(segmentRes[0])];
			//FtrForwardArgs = new wchar_t[wcslen(segmentRes[2])];
			///?????????????????????????????????????
			//函数参数引用指针指向局部wchar_t *
			///?????????????????????????????????????

			KeyWord = segmentRes[1];
			FtrBackArgs = segmentRes[2];
			FtrForwardArgs = segmentRes[0];

			delete new_ci;
			return;
		}else{
			FtrArgsNum = 0;
			return;
		}
		break;
	case 2:
		for (int i = 0; i < InpCiCount; i++)
		{
			new_ci->ci = segmentRes[i];
			new_ci->nEnd = new_ci->nStart = 0;

			searchRes = (Ci2IdxNode*)bsearch(new_ci, g_ftrCi2Idx, g_nCiCount, sizeof(Ci2IdxNode), FTRCompare);
			if (searchRes)
			{
				KeyWord = segmentRes[i];
				switch (i)
				{
				case 0:
					FtrArgsNum = 1;
					FtrBackArgs = segmentRes[InpCiCount-i-1];
					return;
				case 1:
					FtrArgsNum = 2;
					FtrForwardArgs = segmentRes[InpCiCount-i-1];
					return;
				default:
					break;
				}
			}
		}
		FtrArgsNum = 0;
		return;
		break;
	case 1:
		KeyWord = segmentRes[0];
		FtrArgsNum = 4;
		return;
		break;
	default:
		FtrArgsNum = 0;
		return;
		break;
	}

	if (new_ci != NULL){delete new_ci;}
	return;
}

bool FtrLHBF(wchar_t * strKW, wchar_t * strInpBack, wchar_t * strInpForward, int nMaxRetCount, int & nAllResCount, int & nRetCount, BFLhd * &res){
	__int64 nBackStart = 0, nBackEnd = 0, nForwardStart = 0, nForwardEnd = 0;
	char ** psRet = NULL;
	__int64 BackNum = 0, ForwardNum = 0;;
	int MaxFileCount = 1, MaxJuCountPerFile = 0, nBackLHDInHash = 0;
	nAllResCount = 0, nRetCount = 0;

	unordered_map<__int64, int> * hashMapJuId = NULL;				//JuID => BackLHD
	hashMapJuId = new unordered_map<__int64, int>[MaxFileCount];


	if(!FTRLHCore(strKW, strInpBack, psRet, -1, BackNum, nBackStart, nBackEnd, FTRTYPE_BACK)){
		return false;
	}

	if(nBackEnd == -1){
		//adjust nBackEnd from "-1" to "nBackStart + 1"
		//In this case, assume there is only one result which start position is: nForwardStart;
		nBackEnd = nBackStart + 1;
	}

	for (__int64 i = nBackStart; i < nBackEnd; i++){
		hashMapJuId[g_ftrBackIdx[i].DID].insert(unordered_map<__int64, int>::value_type(g_ftrBackIdx[i].JuID, g_ftrBackIdx[i].LHD));
	}

	if(!FTRLHCore(strKW, strInpForward, psRet, -1, ForwardNum, nForwardStart, nForwardEnd, FTRTYPE_FORWARD)){
		return false;
	}

	if(nForwardEnd == -1){	nForwardEnd = nForwardStart + 1; }

	for (__int64 i = nForwardStart; i < nForwardEnd; i++){
		try {
			nBackLHDInHash = hashMapJuId[g_ftrForwardIdx[i].DID].at(g_ftrForwardIdx[i].JuID);
			if (nRetCount >= nMaxRetCount){
				nAllResCount++;
				continue;
			}
			res[nRetCount].nBackLHD = nBackLHDInHash;
			res[nRetCount].nForwardLHD = g_ftrForwardIdx[i].LHD;
			res[nRetCount].nJuID = g_ftrForwardIdx[i].JuID;
			res[nRetCount].nPos = g_ftrForwardIdx[i].POS;
			nRetCount++;nAllResCount++;
		}
		catch ( exception &e ) {
			//If the argument key value is not found, then the function throws an object of class out_of_range.
			continue;
		};
	}

	if (hashMapJuId != NULL){
		delete[] hashMapJuId;
	}

	return true;
}

void PrintRes(__int64 nStart, __int64 nEnd, int psInp1Len, int psInp2Len, FTRTYPE FtrType){
	setlocale(LC_ALL, "");
	__int64 tEnd = 0, tStart = 0;
	int offsetStart = 10, offsetEnd = 10;
	wchar_t curChar;

	for( __int64 i=nStart; i < nEnd; i++){
		offsetStart = offsetEnd = 10;
		if (FtrType == FTRTYPE_FORWARD){
			do{
				tEnd = g_ftrForwardIdx[i].POS + offsetStart--;
			} while (tEnd > g_nTotalCharNum);
			do{
				tStart = g_ftrForwardIdx[i].POS - g_ftrForwardIdx[i].LHD - psInp2Len - offsetEnd--;
			} while (tStart < 0);
		}else if (FtrType == FTRTYPE_BACK){
			do{
				tEnd = g_ftrBackIdx[i].POS + g_ftrBackIdx[i].LHD + psInp2Len + offsetEnd;
			} while (tEnd > g_nTotalCharNum);
			do{
				tStart = g_ftrBackIdx[i].POS - psInp1Len - offsetStart--;
			} while (tStart < 0);

		}else
			return;

		for (__int64 j = tStart; j < tEnd; j++)
		{
			curChar = g_ftrDat[j];
			//if (curChar != 0x20)
			printf("%ls", &curChar);

			if (wcscmp(&curChar, L" ")){
				//printf("%lc", &curChar);
			}
		}
		printf("\n");
	}
	return;
}

bool FTRLH(_TCHAR* argv, int &FtrArgsNum, char* &psInp, char** &psRet, int &nMaxRetLen, __int64 &Num, int &BFRetCount, int &BFAllResCount, __int64 & nRetStart, __int64 & nRetEnd){
	wchar_t *KeyWord=NULL;
	wchar_t *FtrBackArgs = NULL;
	wchar_t *FtrForwardArgs = NULL;

	int printStartPos = 0, printEndPos = 0, printOFFSET = 10;
	BFLhd * BFLhdRes = NULL;

	InpParser(argv, FtrArgsNum, KeyWord, FtrBackArgs, FtrForwardArgs);
	setlocale(LC_ALL, "");
	printf("Now start search...Keyword: %ls\n", KeyWord);
	switch (FtrArgsNum)
	{
	case 4:
		FTRLHCore(KeyWord, L"", psRet, nMaxRetLen, Num, nRetStart, nRetEnd, FTRTYPE_BACK);
		if (Num != 0){
			PrintRes(nRetStart, nRetEnd, wcslen(KeyWord), 0, FTRTYPE_BACK);
		}else
			printf("0 Result return");
		//only ftr no LH;
		break;
	case 3:
		BFLhdRes = new BFLhd[nMaxRetLen];
		FtrLHBF(KeyWord, FtrBackArgs, FtrForwardArgs, nMaxRetLen, BFAllResCount, BFRetCount, BFLhdRes);
		if(BFRetCount){
			//Prompt content:
			printf("Search result:\n");

			setlocale(LC_ALL, "");
			for (int i = 0; i < BFRetCount; i++){
				printOFFSET = 10;
				do{
					printStartPos = BFLhdRes[i].nPos - BFLhdRes[i].nForwardLHD - printOFFSET--;
				} while (printStartPos<0);
				printOFFSET = 10;
				do{
					printEndPos =  BFLhdRes[i].nPos + BFLhdRes[i].nBackLHD + wcslen(FtrBackArgs) + printOFFSET--;
				} while (printEndPos>g_nTotalCharNum);

				for (int j =printStartPos; j < printEndPos; j++){
					wchar_t t = g_ftrDat[j];
					printf("%ls", &t);
				}
				printf("\n");
			}
		}else{
			printf("0 Result return");
		}
		if( BFLhdRes != NULL){
			delete[] BFLhdRes;
		}
		//back & forward;
		break;
	case 2:
		FTRLHCore(KeyWord, FtrForwardArgs, psRet, nMaxRetLen, Num, nRetStart, nRetEnd, FTRTYPE_FORWARD);
		if (Num != 0)
			PrintRes(nRetStart, nRetEnd, wcslen(KeyWord), wcslen(FtrForwardArgs), FTRTYPE_FORWARD);
		else
			printf("0 Result return");
		//only forward;
		break;
	case 1:
		FTRLHCore(KeyWord, FtrBackArgs, psRet, nMaxRetLen, Num, nRetStart, nRetEnd, FTRTYPE_BACK);
		if (Num != 0)
			PrintRes(nRetStart, nRetEnd, wcslen(KeyWord), wcslen(FtrBackArgs), FTRTYPE_BACK);
		else
			printf("0 Result return");
		//only back;
		break;
	case 0:
		printf("Error retrieval string.\n");
		//error
		break;
	default:
		break;
	}
	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc < 2){
		printf(" argv1: mode:{{1, -createIdx},{2, -search},{3, -searchBatch}} \n argv2: {{mode1 segmented corpus folder}, {mode2 inp}} \n argv3: mode1 dict.txt");
		return 0;
	}

	TCHAR curPath[MAX_PATH];
	if( !GetModuleFileName( NULL, curPath, MAX_PATH ) ){
		return FALSE;
	}
	wcsrchr(curPath, '\\')[1]=0;
	char psLHIdxBackOutput[] = ".\\LHIdx\\psLHBackIdx.idx";
	char psLHIdxForwardOutput[] = ".\\LHIdx\\psLHForwardIdx.idx";
	char psLHCi[] = ".\\LHIdx\\psLHCi2Idx.idx";
	char psSegDataOutput[] = ".\\LHIdx\\psSegDataOutput.txt";
	char psTextDataOutput[] = ".\\LHIdx\\psTextDataOutput.txt";

	/*
	argv1: mode:{{1, createIdx},{2, search}}
	argv2: {{mode1 segmented corpus folder}, {mode2 inp}}
	argv3: mode1 dict.txt
	*/
	if(wcscmp(argv[1], L"-createIdx") == 0){
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;

		SetCurrentDirectory(curPath);

		int nFileNum = 0;
		wchar_t* psFiles[MAX_CORPUS_COUNT];

		wchar_t szDir[MAX_PATH];
		wcscpy(szDir, argv[2]);
		wcscat(szDir, TEXT("\\*"));

		hFind = FindFirstFile(szDir, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			printf ("NOT FIND FILE: '%s';\T ERROR CODE: (%d)\n", argv[2], GetLastError());
			return 0;
		}else{
			do{
				if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
					continue;
				}else{
					if (nFileNum > MAX_CORPUS_COUNT){
						printf("%d file indexed, already max corpus number...\n", nFileNum);
						break;
					}
					psFiles[nFileNum] = new wchar_t[MAX_PATH];
					wcscpy_s(psFiles[nFileNum], MAX_PATH, argv[2]);
					wcscat_s(psFiles[nFileNum], MAX_PATH, L"\\");
					wcscat_s(psFiles[nFileNum], MAX_PATH, FindFileData.cFileName);
					nFileNum++;
				}
			}while(FindNextFile(hFind, &FindFileData) != 0);

			FindClose(hFind);
		}

		// Check for folder LHIdx existence.
		TCHAR FolderLHIdx[MAX_PATH];
		wcscpy_s(FolderLHIdx, MAX_PATH, curPath);
		wcscat_s(FolderLHIdx, MAX_PATH, L"LHIdx");

		try{
			if (FindFirstFile(FolderLHIdx, &FindFileData) == INVALID_HANDLE_VALUE) {
				if( _wmkdir( FolderLHIdx ) == 0 )  {
					printf( "Directory '%s' was successfully created\n" , FolderLHIdx);
				}else{
					printf( "Problem creating directory '%s'\n" , FolderLHIdx);
					return 0;
				}
			}else {
				_tprintf (TEXT("Folder already exist: %s\n"), FindFileData.cFileName);
				FindClose(hFind);
			}
		}catch(exception e){
			printf("%s", e);
		}


		printf("Starting create Idx...\n");

		SetCurrentDirectory(curPath);

		//char* psSegData, char* psTextData

		if(CreateIdx(argv[3], psFiles, nFileNum, psSegDataOutput, psTextDataOutput,psLHCi, psLHIdxBackOutput, psLHIdxForwardOutput)){
			printf("Create Idx SUCCESS!\n");
			printf("Total char: %d\n", g_nTotalCharNum);
			printf("Total ci: %d\n", g_nCiCount);
			printf("Total ju: %d\n", g_nJuCount);
		}

		for (int i = 0; i < nFileNum; i++){
			if (psFiles[i] != NULL)
				delete[] psFiles[i];
		}

	}else if(wcscmp(argv[1], L"-search") == 0){
		printf("Reading Idx files...\n");

		SetCurrentDirectory(curPath);
		FTRLHInit(psLHCi, psTextDataOutput, psLHIdxBackOutput, psLHIdxForwardOutput);

		//variable for ftr
		char* psInp = NULL;
		char** psRet = new char*[MAX_RET_NUM];;
		int nMaxRetLen = 10;
		__int64 Num = 0;

		int FtrArgsNum = 0, BFRetCount = 0, BFAllResCount = 0;
		wchar_t *KeyWord=NULL;
		wchar_t *FtrBackArgs = NULL;
		wchar_t *FtrForwardArgs = NULL;
		__int64 nRetStart = 0, nRetEnd = 0;
		int printStartPos = 0, printEndPos = 0, printOFFSET = 10;
		BFLhd * BFLhdRes = NULL;

		FTRLH(argv[2], FtrArgsNum, psInp, psRet, nMaxRetLen, Num, BFRetCount, BFAllResCount, nRetStart, nRetEnd);

		FTRLHExit();
	}else if(wcscmp(argv[1], L"-searchBatch") == 0){
		printf("Reading Idx files...\n");
		SetCurrentDirectory(curPath);
		FTRLHInit(psLHCi, psTextDataOutput, psLHIdxBackOutput, psLHIdxForwardOutput);

		//variable for ftr
		char* psInp = NULL;
		char** psRet = new char*[MAX_RET_NUM];;
		int nMaxRetLen = 10;
		__int64 Num = 0;

		int FtrArgsNum = 0, BFRetCount = 0, BFAllResCount = 0;
		wchar_t *KeyWord=NULL;
		wchar_t *FtrBackArgs = NULL;
		wchar_t *FtrForwardArgs = NULL;
		__int64 nRetStart = 0, nRetEnd = 0;
		int printStartPos = 0, printEndPos = 0, printOFFSET = 10;
		BFLhd * BFLhdRes = NULL;
		wchar_t inp[48];
		int inps_count = 999;
		time_t ltime;time( &ltime );
		wchar_t curChar[2];curChar[1] = '\0';
		setlocale(LC_ALL, "");
		printf("Start search, time_stamp: %lld", (long long)ltime);
		while(inps_count>0){
			try{
				//srand(unsigned(time(0)));
				curChar[0] = g_ftrDat[rand()%g_nTotalCharNum];
				wcscpy_s(inp, curChar);
				wcscat_s(inp, L"*");
				wcscat_s(inp, g_ftrCi2Idx[inps_count].ci);
				wcscat_s(inp, L"*");
				curChar[0] = g_ftrDat[rand()%g_nTotalCharNum];
				wcscat_s(inp, curChar);
				printf("\n%ls\n", inp);

				FTRLH(inp, FtrArgsNum, psInp, psRet, nMaxRetLen, Num, BFRetCount, BFAllResCount, nRetStart, nRetEnd);
				/*search("词表里的词*语料库的一个字");
				search("语料库的一个字*词表里的词");
				search("语料库的一个字*词表里的词*语料库的一个字");*/
				inps_count -= 3;
			}catch(exception e){
				printf("Error: %s", e);
			}
		}
		time( &ltime );
		printf("\nEnd search, time_stamp: %lld", (long long)ltime);
		FTRLHExit();
	}else{
		printf(" argv1: mode:{{1, -createIdx},{2, -search},{3, -searchBatch}} \n argv2: {{mode1 segmented corpus folder}, {mode2 inp}} \n argv3: mode1 dict.txt");
	}
	return 0;

	//-createIdx corpus3 dict_from_bccc.txt
	//-search 我*拥有*喜欢
}
