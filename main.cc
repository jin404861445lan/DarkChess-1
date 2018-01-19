/*****************************************************************************\
 * Theory of Computer Games: Fall 2012
 * Chinese Dark Chess Search Engine Template by You-cheng Syu
 *
 * This file may not be used out of the class unless asking
 * for permission first.
 *
 * Modify by Hung-Jui Chang, December 2013
\*****************************************************************************/
#include<iostream> 
#include<cstdio>
#include<cstdlib>
#include <math.h>
#include <random>
#include"anqi.hh"
#include"Protocol.h"
#include"ClientSocket.h"

#ifdef _WINDOWS
#include<windows.h>
#else
#include<ctime>
#endif

#define HASH_LENGTH 1024*1024

using namespace std;
const int DEFAULTTIME = 20;
typedef  int SCORE;
static const SCORE INF=1000001;
static const SCORE WIN=1000000;
int remain_time;
int eat_move;
unsigned long long int HT[32][32];
unsigned long long int Zobrist[14][32];
unsigned long long int ZobristPlayer = 1234567890;
int pass = 0;
MOVLST PV;
mt19937 mt(01234567);
TRANSPOSITION tran_table[HASH_LENGTH];

unsigned long long int Game_record[128];
int Game_length = 0;

SCORE SearchMax(const BOARD&,int,int,unsigned long long int,int,int);
SCORE SearchMin(const BOARD&,int,int,unsigned long long int,int,int);
unsigned long long int randomInt();
SCORE Quiescent_F(const BOARD&,int,int);
SCORE Quiescent_G(const BOARD&,int,int);
SCORE Quiescence_F(const BOARD&,int,int);
SCORE Quiescence_G(const BOARD&,int,int);
#ifdef _WINDOWS
DWORD Tick;     // 開始時刻
int   TimeOut;  // 時限
#else
clock_t Tick;     // 開始時刻
clock_t TimeOut;  // 時限
#endif
MOV   BestMove,BestMove_temp; // 搜出來的最佳著法

bool TimesUp() {
#ifdef _WINDOWS
	return GetTickCount()-Tick>=TimeOut;
#else
	return clock() - Tick > TimeOut;
#endif
}

MOV run(const BOARD &B) {
	POS ADJ[32][4]={
	{ 1,-1,-1, 4},{ 2,-1, 0, 5},{ 3,-1, 1, 6},{-1,-1, 2, 7},
	{ 5, 0,-1, 8},{ 6, 1, 4, 9},{ 7, 2, 5,10},{-1, 3, 6,11},
	{ 9, 4,-1,12},{10, 5, 8,13},{11, 6, 9,14},{-1, 7,10,15},
	{13, 8,-1,16},{14, 9,12,17},{15,10,13,18},{-1,11,14,19},
	{17,12,-1,20},{18,13,16,21},{19,14,17,22},{-1,15,18,23},
	{21,16,-1,24},{22,17,20,25},{23,18,21,26},{-1,19,22,27},
	{25,20,-1,28},{26,21,24,29},{27,22,25,30},{-1,23,26,31},
	{29,24,-1,-1},{30,25,28,-1},{31,26,29,-1},{-1,27,30,-1}};
	MOVLST lst;
	B.MoveGen(lst);
	MOV run_move = lst.mov[0];
	for(int i=0;i<lst.num;i++)
	{
		BOARD N(B);
		N.Move(lst.mov[i]);
		if(N.ChkLose())
		{
			continue;
		}
		if(lst.mov[i].state == 3)
		{
			run_move = lst.mov[i];
			return run_move;
		}
		for(int j=0;j<4;j++)
		{
			if(ADJ[lst.mov[i].ed][j] == -1 || B.fin[ADJ[lst.mov[i].ed][j]] == 15 || B.fin[ADJ[lst.mov[i].ed][j]] == 5 || B.fin[ADJ[lst.mov[i].ed][j]] == 12)
			{
				continue;
			}
			if(!ChkEats(B.fin[ADJ[lst.mov[i].ed][j]],B.fin[lst.mov[i].st]))
			{
				run_move = lst.mov[i];
				return run_move;
			}
		}
	}
	return run_move;
}


//動態審局 
SCORE Eval(const BOARD &B) {
	int cnt[2]={300,300};
	int not_cnt = 0;
	int board_piece[14]={1,2,2,2,2,2,5,1,2,2,2,2,2,5};
	int dynamic_score[14]={94,47,23,11,5,40,2,94,47,23,11,5,40,2}; //動態分數 
	for (int i = 0 ; i < 14; ++i) 
	{
		not_cnt+= B.overage[i];
    }
    //cout << "未翻開:" << not_cnt << endl; 
    dynamic_score[5] = dynamic_score[12] = not_cnt+8;
	if(B.overage[0]==0) //帥死
		dynamic_score[13] = 1;
	else
	{
		if(B.overage[13] < 4)
		{
			dynamic_score[13] = dynamic_score[8+B.overage[13]];
		}
	}
    if(B.overage[7]==0) //將死
		dynamic_score[6]=1;
	else
	{
		if(B.overage[6] < 4)
		{
			dynamic_score[6] = dynamic_score[1+B.overage[6]];
		}
	}

	for(int i=0;i<14;i++)
	{
		if(B.overage[i] != board_piece[i])
			cnt[i/7] = cnt[i/7]-(board_piece[i]-B.overage[i])*dynamic_score[i];
	}
	return cnt[B.who]-cnt[B.who^1];
}

SCORE Quiescent_F(const BOARD &B,int alpha,int beta) {
	//cout << "Quiescent_F" << endl;
	MOVLST lst,lst_quiescent;
	B.MoveGen(lst);
	SCORE ret;
	int len = 0;
	for(int i=0;i<lst.num;i++)
	{
		if(lst.mov[i].state != 0)
		{
			lst_quiescent.mov[len] = lst.mov[i];
			lst_quiescent.mov[len].state = lst.mov[i].state;
			lst_quiescent.num = ++len;
		}
	}
	/*cout << "我方走法: " << endl;
	for(int i=0;i<lst_quiescent.num;i++)
	{
		cout << i+1 << ". " << lst_quiescent.mov[i].st << "->" << lst_quiescent.mov[i].ed << ",狀態:" << lst_quiescent.mov[i].state << endl;
	}
	cout << endl;*/
	if(lst_quiescent.num == 0)
	{
		if(pass == 0)
		{
			//cout << "我方pass,換對方:" << endl;
			pass = 1;
			BOARD N(B);
			N.Move(MOV(-1,-1));
			N.Display();
			return Quiescent_G(N, alpha, beta);
		}
		else
		{
			//cout << "對方pass過,我方也pass,結束" << endl; 
			pass = 0;
			B.Display();
			return +Eval(B);
		}
	}
	else
	{
		pass = 0;
		ret= -INF;
		for(int i=0;i<lst_quiescent.num;i++)
		{
			BOARD N(B);
			N.Move(lst_quiescent.mov[i]);
			cout << i+1 << ". " << lst_quiescent.mov[i].st << "->" << lst_quiescent.mov[i].ed << endl;
			N.Display();
			int temp = Quiescent_G(N, ret>alpha?ret:alpha, beta);
			if(ret < temp)
			{
				ret = temp;
			}
			if(ret >= beta) //Beta cut-off
			{
				return ret;
			}
		}
		return ret;
	}
	
}

SCORE Quiescent_G(const BOARD &B,int alpha,int beta) {
	//cout << "Quiescent_G" << endl;
	MOVLST lst,lst_quiescent;
	B.MoveGen(lst);
	SCORE ret;
	int len = 0;
	for(int i=0;i<lst.num;i++)
	{
		if(lst.mov[i].state != 0)
		{
			lst_quiescent.mov[len] = lst.mov[i];
			lst_quiescent.mov[len].state = lst.mov[i].state;
			lst_quiescent.num = ++len;
		}
	}
	/*cout << "對方走法: " << endl;
	for(int i=0;i<lst_quiescent.num;i++)
	{
		cout << i+1 << ". " << lst_quiescent.mov[i].st << "->" << lst_quiescent.mov[i].ed << ",狀態:" << lst_quiescent.mov[i].state << endl;
	}
	cout << endl;*/
	if(lst_quiescent.num == 0)
	{
		if(pass == 0)
		{
			//cout << "對方pass,換我方:" << endl;
			pass = 1;
			BOARD N(B);
			N.Move(MOV(-1,-1));
			N.Display();
			return -Quiescent_F(N, alpha, beta);
		}
		else
		{
			//cout << "我方pass過,對方也pass,結束" << endl; 
			pass = 0;
			B.Display();
			return -Eval(B);
		}
	}
	else
	{
		pass = 0;
		ret= +INF;
		for(int i=0;i<lst_quiescent.num;i++)
		{
			BOARD N(B);
			N.Move(lst_quiescent.mov[i]);
			cout << i+1 << ". " << lst_quiescent.mov[i].st << "->" << lst_quiescent.mov[i].ed << endl;
			N.Display();
			int temp = Quiescent_F(N, alpha, ret<beta?ret:beta);
			if(ret > temp)
			{
				ret = temp;
			}
			if(ret <= alpha) //Alpha cut-off
			{
				return ret;
			}
		}
		return ret;
	}
	
}

// dep=現在在第幾層
// cut=還要再走幾層
// NegaScout F4'
SCORE SearchMax(const BOARD &B,int alpha,int beta,unsigned long long int ZobristKey,int dep,int cut) {
	
	//cout << "SearchMax" << endl;
	if(B.ChkLose())
		return -WIN;
	
	MOVLST lst_temp;
	if(cut == 0 || TimesUp() || B.MoveGen(lst_temp) == 0)
	{
		//cout << "SearchMax Final->分數:" << Eval(B) << endl;
		//B.Display();
		//tran_table[ZobristKey%HASH_LENGTH].value = Quiescence_F(B,alpha,beta);
		return +Eval(B);
	}
	
	/*cout << "Max原走法: " << endl;
	for(int i=0;i<lst_temp.num;i++)
	{
		cout << i+1 << ". " << lst_temp.mov[i].st << "->" << lst_temp.mov[i].ed << " ,分數:" << HT[lst_temp.mov[i].st][lst_temp.mov[i].ed] << " ,狀態:" << lst_temp.mov[i].state << endl;
	}
	cout << endl;*/
	
	bool re_test[lst_temp.num];
	for(int i=0;i<lst_temp.num;i++)
	{
		
		unsigned long long int Zobrist_temp = ZobristKey;
		BOARD re_board(B);
		re_board.Move(lst_temp.mov[i]);
		if(lst_temp.mov[i].ed == -1 && lst_temp.mov[i].st == -1)
		{
			continue;
			//Zobrist_temp ^= ZobristPlayer;
		}
		else
		{
			if(B.fin[lst_temp.mov[i].ed] != 15) // 吃子 
			{
				Zobrist_temp ^= Zobrist[B.fin[lst_temp.mov[i].st]][lst_temp.mov[i].st]^Zobrist[B.fin[lst_temp.mov[i].ed]][lst_temp.mov[i].ed]^Zobrist[B.fin[lst_temp.mov[i].st]][lst_temp.mov[i].ed];
				//Zobrist_temp ^= ZobristPlayer;
			}
			else //移動 
			{
				Zobrist_temp ^= Zobrist[B.fin[lst_temp.mov[i].st]][lst_temp.mov[i].st]^Zobrist[B.fin[lst_temp.mov[i].st]][lst_temp.mov[i].ed];
				//Zobrist_temp ^= ZobristPlayer;
			}
		}
		Zobrist_temp ^= ZobristPlayer;
		//cout << i+1 << ". " << lst_temp.mov[i].st << "->" << lst_temp.mov[i].ed << "Zobrist:" << Zobrist_temp << endl;
		int test = 0;
		for(int len=0;len<Game_length;len++)
		{
			if(Game_record[len] == Zobrist_temp)
			{
				//cout << "發現重複盤面" << endl; 
				test = 1;
				break;
			}
		}
		if(test == 1) 
		{
			re_test[i] = 1;
		}
		else
		{
			re_test[i] = 0;
		}
	}
	MOVLST lst;
	int num = 0;
	for(int i=0;i<lst_temp.num;i++)
	{
		if(re_test[i] == 0)
		{
			lst.mov[num] = lst_temp.mov[i];
			lst.mov[num].state = lst_temp.mov[i].state;
			num++;
		}
	}
	lst.num = num;
	if(lst.num == 0)
	{
		return +Eval(B);
	}
	/*cout << "數量" << lst.num << ",Max新走法: " << endl;
	for(int i=0;i<lst.num;i++)
	{
		cout << i+1 << ". " << lst.mov[i].st << "->" << lst.mov[i].ed << " ,分數:" << HT[lst.mov[i].st][lst.mov[i].ed] << " ,狀態:" << lst.mov[i].state << endl;
	}
	cout << endl;*/
	for(int i=0;i<lst.num;i++) //History Heuristic
	{
		int pos = i;
		unsigned long long int max = HT[lst.mov[i].st][lst.mov[i].ed];
		for(int j=i+1;j<lst.num;j++)
		{
			if(max < HT[lst.mov[j].st][lst.mov[j].ed])
			{
				max = HT[lst.mov[i].st][lst.mov[i].ed];
				pos = j;
			}
		}
		MOV temp;
		temp.st = lst.mov[pos].st;
		lst.mov[pos].st = lst.mov[i].st;
		lst.mov[i].st = temp.st;
		temp.ed = lst.mov[pos].ed;
		lst.mov[pos].ed = lst.mov[i].ed;
		lst.mov[i].ed = temp.ed;
		temp.state = lst.mov[pos].state;
		lst.mov[pos].state = lst.mov[i].state;
		lst.mov[i].state = temp.state;
	}
	
	if(lst.num == 1 && lst.mov[0].st == -1 && lst.mov[0].ed == -1)
	{
		int ret = alpha;
		int temp = 0;
		if(pass == 0)
		{
			pass = 1;
			BOARD N(B);
			N.Move(MOV(-1,-1));
			unsigned long long int Zobrist_temp = ZobristKey;
			Zobrist_temp ^= ZobristPlayer;
			//tran_table[ZobristKey%HASH_LENGTH].value = SearchMin(N,alpha,beta,Zobrist_temp,dep+1,cut-1);
			//temp = SearchMin(N,alpha,beta,Zobrist_temp,dep+1,cut-1);
			return SearchMin(N,alpha,beta,Zobrist_temp,dep+1,cut-1);
		}
		else
		{
			pass = 0;
			//tran_table[ZobristKey%HASH_LENGTH].value = +Eval(B);
			return +Eval(B);
		}
	}
	else
	{
		if(pass == 1)
		{
			int ret = +Eval(B);
			int temp = 0;
			//cout << "F3" << endl;
			for(int i=0;i<lst.num;i++)
			{
				if(lst.mov[i].state == 1)
				{
					pass = 0;
					BOARD N(B);
					N.Move(lst.mov[i]);
					//cout << "吃子步:" <<  lst.mov[i].st << "->" << lst.mov[i].ed << endl;
					unsigned long long int Zobrist_temp = ZobristKey;
					Zobrist_temp ^= Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].st]^Zobrist[B.fin[lst.mov[i].ed]][lst.mov[i].ed]^Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].ed];
					Zobrist_temp ^= ZobristPlayer;
					temp = SearchMin(N,ret,beta,Zobrist_temp,dep+1,cut-1);
					if(ret < temp)
					{
						ret = temp;
					}
					if(ret > beta) //Beta cut-off
					{
						return ret;
					}
				}
			}
			return ret;
		}
	}
	
	SCORE ret=-INF;
	if(tran_table[ZobristKey%HASH_LENGTH].hit && tran_table[ZobristKey%HASH_LENGTH].key == ZobristKey) //Hash hit
	{
		//cout << "SearchMax Hash hit!!!" << endl;
		if(cut <= tran_table[ZobristKey%HASH_LENGTH].depth)
		{
			if(tran_table[ZobristKey%HASH_LENGTH].exact == 1) //exact solution
			{
				BestMove_temp=tran_table[ZobristKey%HASH_LENGTH].BestMove;
				BestMove_temp.state = tran_table[ZobristKey%HASH_LENGTH].BestMove.state;
				return tran_table[ZobristKey%HASH_LENGTH].value;
			}
			else if(tran_table[ZobristKey%HASH_LENGTH].exact == 0) //lower bound
			{
				if(alpha < tran_table[ZobristKey%HASH_LENGTH].value)
				{
					alpha = tran_table[ZobristKey%HASH_LENGTH].value;
					if(alpha >= beta) //Beta cut-off
					{
						return alpha;
					}
				}
			}
			else
			{
				if(beta > tran_table[ZobristKey%HASH_LENGTH].value) //upper bound
				{
					beta = tran_table[ZobristKey%HASH_LENGTH].value;
					if(beta <= alpha) //Alpha cut-off
					{
						return beta;
					}
				}
			}
		}
		else
		{
			if(tran_table[ZobristKey%HASH_LENGTH].exact == 1)
			{
				ret = tran_table[ZobristKey%HASH_LENGTH].value;
			}
		}
	}
	else
	{
		tran_table[ZobristKey%HASH_LENGTH].hit = 1;
		tran_table[ZobristKey%HASH_LENGTH].key = ZobristKey;
	}
	tran_table[ZobristKey%HASH_LENGTH].depth = cut;
	
	unsigned long long int Zobrist_temp = ZobristKey;
	
	BOARD first_branch(B);
	first_branch.Move(lst.mov[0]);
	if(lst.mov[0].ed == -1 && lst.mov[0].st == -1)
	{
		Zobrist_temp ^= ZobristPlayer;
	}
	else
	{
		if(B.fin[lst.mov[0].ed] != 15) // 吃子 
		{
			Zobrist_temp ^= Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].st]^Zobrist[B.fin[lst.mov[0].ed]][lst.mov[0].ed]^Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].ed];
			Zobrist_temp ^= ZobristPlayer;
		}
		else //移動 
		{
			Zobrist_temp ^= Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].st]^Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].ed];
			Zobrist_temp ^= ZobristPlayer;
		}
	}
	
	if(dep == 0)
	{
		//cout << "原BestMove_temp=" << BestMove_temp.st << "->" << BestMove_temp.ed << endl;
		BestMove_temp=lst.mov[0];
		BestMove_temp.state = lst.mov[0].state;
		if(lst.mov[0].state == 1) //吃子步 
		{
			eat_move = SearchMin(first_branch,alpha,beta,Zobrist_temp,0,4);
		}
		tran_table[ZobristKey%HASH_LENGTH].BestMove = lst.mov[0];
		tran_table[ZobristKey%HASH_LENGTH].BestMove.state = lst.mov[0].state;
		//cout << "更改後BestMove_temp=" << BestMove_temp.st << "->" << BestMove_temp.ed << endl;
	}
	
	SCORE temp = SearchMin(first_branch,alpha,beta,Zobrist_temp,dep+1,cut-1);
	if(ret < temp) // the first branch
	{
		ret = temp;
	}
	
	int where = 0; //where is the best child index
	if(ret >= beta) //Beta cut-off
	{
		tran_table[ZobristKey%HASH_LENGTH].value = ret;   //update this value as a lower bound into the transposition table
		tran_table[ZobristKey%HASH_LENGTH].exact = 0;
		if(lst.mov[0].ed != -1 && lst.mov[0].st != -1)
			HT[lst.mov[0].st][lst.mov[0].ed] += 2*cut;
		return ret;
	}
	
	int max = ret;
	for(int i=1;i<lst.num;i++)
	{
		BOARD N(B);
		N.Move(lst.mov[i]);

		Zobrist_temp = ZobristKey;
		if(lst.mov[i].ed == -1 && lst.mov[i].st == -1)
		{
			pass = 1;
			Zobrist_temp ^= ZobristPlayer;
		}
		else
		{
			if(B.fin[lst.mov[i].ed] != 15) // 吃子 
			{
				Zobrist_temp ^= Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].st]^Zobrist[B.fin[lst.mov[i].ed]][lst.mov[i].ed]^Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].ed];
				Zobrist_temp ^= ZobristPlayer;
			}
			else //移動 
			{
				Zobrist_temp ^= Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].st]^Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].ed];
				Zobrist_temp ^= ZobristPlayer;
			}
		}
		Game_record[Game_length++] = Zobrist_temp;
		
		const SCORE tmp = SearchMin(N,ret,ret+1,Zobrist_temp,dep+1,cut-1);
		if(dep == 0)
		{
			//cout << "原BestMove=" << BestMove_temp.st << "->" << BestMove_temp.ed << ",分數:" << ret << endl;
			//cout << "此步分數=" << lst.mov[i].st << "->" << lst.mov[i].ed << ",分數:" << tmp << endl;
		}
		if(tmp > ret) // failed-high
		{
			if(tmp > ret)
			{
				where = i; // where is the best child index
				if(cut < 3 || tmp >= beta)
				{
					ret = tmp;
				}
				else
				{
					ret = SearchMin(N,tmp,beta,Zobrist_temp,dep+1,cut-1);
				}
			}
			PV.mov[dep] = lst.mov[i];
			PV.num = dep;
			if(dep == 0)
			{
				BestMove_temp=lst.mov[i];
				BestMove_temp.state = lst.mov[i].state;
				tran_table[ZobristKey%HASH_LENGTH].BestMove = lst.mov[i];
				tran_table[ZobristKey%HASH_LENGTH].BestMove.state = lst.mov[i].state;
			}
		}
		Game_length--;
		
		if(ret >= beta) //Beta cut-off
		{
			tran_table[ZobristKey%HASH_LENGTH].value = ret;   //update this value as a lower bound into the transposition table
			tran_table[ZobristKey%HASH_LENGTH].exact = 0;
			if(lst.mov[i].ed != -1 && lst.mov[i].st != -1)
				HT[lst.mov[i].st][lst.mov[i].ed] += 2*cut;
			return ret;
		}
	}
	
	if(ret > beta) //Beta cut-off
	{
		tran_table[ZobristKey%HASH_LENGTH].value = ret;
		tran_table[ZobristKey%HASH_LENGTH].exact = 1;
		if(lst.mov[where].ed != -1 && lst.mov[where].st != -1)
			HT[lst.mov[where].st][lst.mov[where].ed] += 2*cut; 
		return ret;
	}
	else
	{
		tran_table[ZobristKey%HASH_LENGTH].value = ret;   //update this value as a upper bound into the transposition table
		tran_table[ZobristKey%HASH_LENGTH].exact = 2;
	}
	//cout << "SearchMax[" << dep << "]=" << ret << endl;
	return ret;
}


// G4'
SCORE SearchMin(const BOARD &B,int alpha,int beta,unsigned long long int ZobristKey,int dep,int cut) {
	
	//cout << "SearchMin" << endl;
	if(B.ChkLose())
		return +WIN;

	MOVLST lst;
	if(cut==0 || TimesUp() || B.MoveGen(lst) == 0)
	{
		//cout << "SearchMin Final->分數:" << -Eval(B) << endl;
		//B.Display();
		//tran_table[ZobristKey%HASH_LENGTH].value = Quiescence_G(B,alpha,beta);
		return -Eval(B);
	}
	/*cout << "Min原走法: " << endl;
	for(int i=0;i<lst.num;i++)
	{
		cout << i+1 << ". " << lst.mov[i].st << "->" << lst.mov[i].ed << " ,分數:" << HT[lst.mov[i].st][lst.mov[i].ed] << " ,狀態:" << lst.mov[i].state << endl;
	}
	cout << endl;*/
	for(int i=0;i<lst.num;i++) //History Heuristic
	{
		int pos = i;
		unsigned long long int max = HT[lst.mov[i].st][lst.mov[i].ed];
		for(int j=i+1;j<lst.num;j++)
		{
			if(max < HT[lst.mov[j].st][lst.mov[j].ed])
			{
				max = HT[lst.mov[i].st][lst.mov[i].ed];
				pos = j;
			}
		}
		MOV temp;
		temp.st = lst.mov[pos].st;
		lst.mov[pos].st = lst.mov[i].st;
		lst.mov[i].st = temp.st;
		temp.ed = lst.mov[pos].ed;
		lst.mov[pos].ed = lst.mov[i].ed;
		lst.mov[i].ed = temp.ed;
		temp.state = lst.mov[pos].state;
		lst.mov[pos].state = lst.mov[i].state;
		lst.mov[i].state = temp.state;
	}
	/*cout << "Min新走法: " << endl;
	for(int i=0;i<lst.num;i++)
	{
		cout << i+1 << ". " << lst.mov[i].st << "->" << lst.mov[i].ed << " ,分數:" << HT[lst.mov[i].st][lst.mov[i].ed] << " ,狀態:" << lst.mov[i].state << endl;
	}
	cout << endl;*/
	
	if(lst.num == 1 && lst.mov[0].st == -1 && lst.mov[0].ed == -1)
	{
		int ret = beta;
		int temp = 0;
		if(pass == 0)
		{
			pass = 1;
			BOARD N(B);
			N.Move(MOV(-1,-1));
			unsigned long long int Zobrist_temp = ZobristKey;
			Zobrist_temp ^= ZobristPlayer;
			//tran_table[ZobristKey%HASH_LENGTH].value = SearchMax(N,alpha,beta,Zobrist_temp,dep+1,cut-1);
			//temp = SearchMax(N,alpha,beta,Zobrist_temp,dep+1,cut-1);
			return SearchMax(N,alpha,beta,Zobrist_temp,dep+1,cut-1);
		}
		else
		{
			pass = 0;
			//tran_table[ZobristKey%HASH_LENGTH].value = -Eval(B);
			return -Eval(B);
		}
	}
	else
	{
		if(pass == 1)
		{
			//cout << "G3" << endl;
			int ret = -Eval(B);
			int temp = 0;
			for(int i=0;i<lst.num;i++)
			{
				if(lst.mov[i].state == 1)
				{
					pass = 0;
					BOARD N(B);
					N.Move(lst.mov[i]);
					unsigned long long int Zobrist_temp = ZobristKey;
					Zobrist_temp ^= Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].st]^Zobrist[B.fin[lst.mov[i].ed]][lst.mov[i].ed]^Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].ed];
					Zobrist_temp ^= ZobristPlayer;
					int temp = SearchMax(N,alpha,ret,Zobrist_temp,dep+1,cut-1);
					if(ret > temp)
					{
						ret = temp;
					}
					if(ret < alpha)
					{
						return ret;
					}
				}
			}
			pass = 0;
			//cout << "G33:" << ZobristKey << endl;
			return ret;
		}
	}
	
	SCORE ret=+INF;
	if(tran_table[ZobristKey%HASH_LENGTH].hit && tran_table[ZobristKey%HASH_LENGTH].key == ZobristKey) //Hash hit
	{
		//cout << "SearchMin Hash hit!!!" << endl;
		if(cut <= tran_table[ZobristKey%HASH_LENGTH].depth)
		{
			if(tran_table[ZobristKey%HASH_LENGTH].exact == 1) //exact solution
			{
				
				return tran_table[ZobristKey%HASH_LENGTH].value;
			}
			else if(tran_table[ZobristKey%HASH_LENGTH].exact == 0) //lower bound
			{
				if(alpha < tran_table[ZobristKey%HASH_LENGTH].value)
				{
					alpha = tran_table[ZobristKey%HASH_LENGTH].value;
					if(alpha >= beta) //Beta cut-off
					{
						return alpha;
					}
				}
			}
			else
			{
				if(beta > tran_table[ZobristKey%HASH_LENGTH].value) //upper bound
				{
					beta = tran_table[ZobristKey%HASH_LENGTH].value;
					if(beta <= alpha) //Alpha cut-off
					{
						return beta;
					}
				}
			}
		}
		else
		{
			if(tran_table[ZobristKey%HASH_LENGTH].exact == 1)
			{
				ret = tran_table[ZobristKey%HASH_LENGTH].value;
			}
		}
	}
	else
	{
		tran_table[ZobristKey%HASH_LENGTH].hit = 1;
		tran_table[ZobristKey%HASH_LENGTH].key = ZobristKey;
	}
	tran_table[ZobristKey%HASH_LENGTH].depth = cut;
	
	unsigned long long int Zobrist_temp = ZobristKey;

	BOARD first_branch(B);
	first_branch.Move(lst.mov[0]);
	if(lst.mov[0].ed == -1 && lst.mov[0].st == -1)
	{
		Zobrist_temp ^= ZobristPlayer;
	}
	else
	{
		if(B.fin[lst.mov[0].ed] != 15) // 吃子 
		{
			Zobrist_temp ^= Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].st]^Zobrist[B.fin[lst.mov[0].ed]][lst.mov[0].ed]^Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].ed];
			Zobrist_temp ^= ZobristPlayer;
		}
		else //移動 
		{
			Zobrist_temp ^= Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].st]^Zobrist[B.fin[lst.mov[0].st]][lst.mov[0].ed];
			Zobrist_temp ^= ZobristPlayer;
		}
	}
	
	SCORE temp = SearchMax(first_branch,alpha,beta,Zobrist_temp,dep+1,cut-1);
	if(ret > temp) // the first branch
	{
		ret = temp;
	}
	
	int where = 0; //where is the best child index
	if(ret <= alpha) //Alpha cut-off
	{
		tran_table[ZobristKey%HASH_LENGTH].value = ret;   //update this value as a upper bound into the transposition table
		tran_table[ZobristKey%HASH_LENGTH].exact = 2;
		if(lst.mov[0].ed != -1 && lst.mov[0].st != -1)
			HT[lst.mov[0].st][lst.mov[0].ed] += 2*cut;
		return ret;
	}
	
	for(int i=1;i<lst.num;i++) 
	{
		BOARD N(B);
		N.Move(lst.mov[i]);
		Zobrist_temp = ZobristKey;
		if(lst.mov[i].st == -1 && lst.mov[i].ed == -1)
		{
			pass = 1;
			Zobrist_temp ^= ZobristPlayer;
		}
		else
		{
			if(B.fin[lst.mov[i].ed] != 15) // 吃子 
			{
				Zobrist_temp ^= Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].st]^Zobrist[B.fin[lst.mov[i].ed]][lst.mov[i].ed]^Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].ed];
				Zobrist_temp ^= ZobristPlayer;
			}
			else //移動 
			{
				Zobrist_temp ^= Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].st]^Zobrist[B.fin[lst.mov[i].st]][lst.mov[i].ed];
				Zobrist_temp ^= ZobristPlayer;
			}
		}
		
		const SCORE tmp = SearchMax(N,ret-1,ret,Zobrist_temp,dep+1,cut-1);
		if(tmp < ret)
		{
			where = i; // where is the best child index
			if(cut < 3 || tmp <= alpha) //Alpha cut-off
			{
				ret = tmp;
			}
			else
			{
				ret = SearchMax(N,alpha,tmp,Zobrist_temp,dep+1,cut-1);
			}
			PV.mov[dep] = lst.mov[i];
			PV.num = dep;
		}
		if(ret <= alpha) //Alpha cut-off
		{
			tran_table[ZobristKey%HASH_LENGTH].value = ret;   //update this value as a upper bound into the transposition table
			tran_table[ZobristKey%HASH_LENGTH].exact = 2;
			if(lst.mov[i].ed != -1 && lst.mov[i].st != -1)
				HT[lst.mov[i].st][lst.mov[i].ed] += 2*cut; 
			return ret;
		}
	}
	
	if(ret < alpha) //Alpha cut-off
	{
		tran_table[ZobristKey%HASH_LENGTH].value = ret;
		tran_table[ZobristKey%HASH_LENGTH].exact = 1;
		if(lst.mov[where].ed != -1 && lst.mov[where].st != -1)
			HT[lst.mov[where].st][lst.mov[where].ed] += 2*cut; 
		return ret;
	}
	else
	{
		tran_table[ZobristKey%HASH_LENGTH].value = ret;   //update this value as a lower bound into the transposition table
		tran_table[ZobristKey%HASH_LENGTH].exact = 0;
	}
	return ret;
}

POS Flip_where(const BOARD &B,unsigned long long int ZobristKey) {
	
	int Board[32] = { 2,3,3,2,
					  3,4,4,3,
					  3,4,4,3,
					  3,4,4,3,
					  3,4,4,3,
					  3,4,4,3,
					  3,4,4,3,
					  2,3,3,2 };
	
	int not_cnt = 0;
	int board_piece[14]={1,2,2,2,2,2,5,1,2,2,2,2,2,5};
	int dynamic_score[14]={94,47,23,11,5,40,2,94,47,23,11,5,40,2}; //動態分數 
	int score[14];
	for (int i = 0 ; i < 14; ++i) 
	{
		not_cnt+= B.overage[i];
    }
    //cout << "未翻開:" << not_cnt << endl; 
    dynamic_score[5] = dynamic_score[12] = not_cnt+8;
	if(B.overage[0]==0) //帥死
		dynamic_score[13] = 1;
	else
	{
		if(B.overage[13] < 4)
		{
			dynamic_score[13] = dynamic_score[8+B.overage[13]];
		}
	}
    if(B.overage[7]==0) //將死
		dynamic_score[6]=1;
	else
	{
		if(B.overage[6] < 4)
		{
			dynamic_score[6] = dynamic_score[1+B.overage[6]];
		}
	}
	
	/*cout << "動態分數:" << endl;
	for(int i=0;i<14;i++)
	{
		cout << dynamic_score[i] << " ";
	}
	cout << endl;*/
	
	unsigned long long int Zobrist_temp = ZobristKey;
	FIN fin[14] = { FIN_K,FIN_G,FIN_M,FIN_R,FIN_N,FIN_C,FIN_P,FIN_k,FIN_g,FIN_m,FIN_r,FIN_n,FIN_c,FIN_p};
	int now_score = SearchMax(B,-INF,INF,ZobristKey,0,4);
	
	for(POS p=0;p<32;p++)
	{
		if(B.fin[p] != FIN_X) //已翻開
		{
			Board[p] = -INF;
		}
		else //未翻開
		{
			int max = -INF;
			POS position = 0;
			for(int i=0;i<14;i++)
			{
				if(B.cnt[i] != 0)
				{
					BOARD N(B);
					N.Flip(p,fin[i]);
					//N.Display();
					pass = 1;
					Zobrist_temp ^= Zobrist[i][p];
					Zobrist_temp ^= ZobristPlayer;
					int temp = SearchMin(N,-INF,INF,Zobrist_temp,0,4);
					//cout << "獲得分數:" << B.cnt[i]*temp << endl;
					Board[p] += B.cnt[i]*(temp-now_score);
				}
			}
		}
	}
	pass = 0;
	int max = -INF;
	POS position = 0;
	for(POS p=0;p<32;p++)
	{
		if(max < Board[p])
		{
			position = p;
			max = Board[p];
		}
	}
	
	/*for(int i=0;i<14;i++)
	{
		if(B.cnt[i] != 0)
		{
			BOARD N(B);
			N.Flip(26,fin[i]);
			N.Display();
			pass = 0;
			Zobrist_temp ^= Zobrist[i][26];
			Zobrist_temp ^= ZobristPlayer;
			int temp = SearchMin(N,-INF,INF,Zobrist_temp,0,4);
			cout << "獲得分數:" << B.cnt[i]*(temp-now_score) << endl;
		}
	}*/
	
	/*for(int i=0;i<14;i++)
	{
		if(B.cnt[i] != 0)
		{
			BOARD N(B);
			N.Flip(position,fin[i]);
			N.Display();
			Zobrist_temp ^= Zobrist[i][position];
			Zobrist_temp ^= ZobristPlayer;
			int temp = SearchMin(N,-INF,INF,Zobrist_temp,0,4);
			cout << "獲得分數:" << B.cnt[i]*(temp-now_score) << endl;
		}
	}*/
	
	/*cout << "盤面分數:";
	for(POS p=0;p<32;p++)
	{
		if(p%4==0)
		{
			cout << endl;
		}
		cout << Board[p] << "\t";
	}*/
	
	return position;
}

int abs(int x,int y)
{
	if(x>y)
		return x-y;
	else
		return y-x;
}

MOV absolute_eat_move(const BOARD &B,int enemy_count) {
	POS ADJ[32][4]={
	{ 1,-1,-1, 4},{ 2,-1, 0, 5},{ 3,-1, 1, 6},{-1,-1, 2, 7},
	{ 5, 0,-1, 8},{ 6, 1, 4, 9},{ 7, 2, 5,10},{-1, 3, 6,11},
	{ 9, 4,-1,12},{10, 5, 8,13},{11, 6, 9,14},{-1, 7,10,15},
	{13, 8,-1,16},{14, 9,12,17},{15,10,13,18},{-1,11,14,19},
	{17,12,-1,20},{18,13,16,21},{19,14,17,22},{-1,15,18,23},
	{21,16,-1,24},{22,17,20,25},{23,18,21,26},{-1,19,22,27},
	{25,20,-1,28},{26,21,24,29},{27,22,25,30},{-1,23,26,31},
	{29,24,-1,-1},{30,25,28,-1},{31,26,29,-1},{-1,27,30,-1}};
	if(enemy_count == 1) //敵方只剩一隻 
	{
		int enemy_pos;
		int my_pos;
		int my_x;
		int my_y;
		int enemy_x;
		int enemy_y;
		int Manhattan = 100;
		for(POS p=0;p<32;p++)
		{

			if(GetColor(B.fin[p]) == B.who^1 && B.fin[p] < 14)
			{
				enemy_pos = p;
				//cout << "敵方位置:" << p << ",子力:" << GetLevel(B.fin[enemy_pos]) << endl; 
				break;
			}
		}
		enemy_x = enemy_pos/4;
		enemy_y = enemy_pos%4;
		int test = 0;
		for(POS p=0;p<32;p++)
		{
			if(GetColor(B.fin[p]) == B.who && B.fin[p] < 14 && ChkEats(B.fin[p],B.fin[enemy_pos]))
			{
				//cout << "我方子力:" << B.fin[p] << ",敵方子力:" << B.fin[enemy_pos] << endl; 
				int temp = p;
				my_x = temp/4;
				my_y = temp%4;
				if(GetLevel(B.fin[p]) != LVL_C && ( Manhattan > abs(my_x,enemy_x)+abs(my_y,enemy_y)) )
				{
					Manhattan = abs(my_x,enemy_x)+abs(my_y,enemy_y);
					my_pos = p;
					test = 1;
				}
			}
		}
		my_x = my_pos/4;
		my_y = my_pos%4;
		if(test == 0)
		{
			//cout << "開始跑步" << endl; 
			return run(B);
		}
		if(Manhattan == 1)
		{
			//cout << "必殺吃子步:" << my_pos << "->" << enemy_pos << endl;
			return MOV(my_pos,enemy_pos);
		}
		else
		{
			if(abs(my_x,enemy_x) == 1 && abs(my_y,enemy_y) == 1)
			{
				for(POS p=0;p<32;p++)
				{
					if(GetColor(B.fin[p]) == B.who && p != my_pos)
					{
						for(int i=0;i<4;i++)
						{
							if(ADJ[p][i] != -1 && B.fin[ADJ[p][i]] == 15)
							{
								//cout << "必殺步:" << p << "->" << ADJ[p][i] << endl;
								return MOV(p,ADJ[p][i]);
							}
						}
					}
				}
			}
			else
			{
				if(abs(my_x,enemy_x) > abs(my_y,enemy_y))
				{
					for(int i=3;i>=0;i--)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							enemy_x = enemy_pos/4;
							enemy_y = enemy_pos%4;
							my_x = ADJ[my_pos][i]/4;
							my_y = ADJ[my_pos][i]%4;
							if(Manhattan >= abs(my_x,enemy_x)+abs(my_y,enemy_y))
							{
								//cout << "必殺步:" << my_pos << "->" << ADJ[my_pos][i] << endl;
								return MOV(my_pos,ADJ[my_pos][i]);
							}
						}
					}
					if(my_x > enemy_x)
					{
						my_pos -= 4;
					}
					else
					{
						my_pos += 4;
					}
					for(int i=0;i<4;i++)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							//cout << "移開棋子" << my_pos << "->" << ADJ[my_pos][i] << endl;
							return MOV(my_pos,ADJ[my_pos][i]);
						}
					}
					
				}
				else
				{
					for(int i=0;i<4;i++)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							enemy_x = enemy_pos/4;
							enemy_y = enemy_pos%4;
							my_x = ADJ[my_pos][i]/4;
							my_y = ADJ[my_pos][i]%4;
							if(Manhattan >= abs(my_x,enemy_x)+abs(my_y,enemy_y))
							{
								//cout << "必殺步:" << my_pos << "->" << ADJ[my_pos][i] << endl;
								return MOV(my_pos,ADJ[my_pos][i]);
							}
						}
					}
					if(my_y > enemy_y)
					{
						my_pos -= 1;
					}
					else
					{
						my_pos += 1;
					}
					for(int i=0;i<4;i++)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							//cout << "移開棋子" << my_pos << "->" << ADJ[my_pos][i] << endl;
							return MOV(my_pos,ADJ[my_pos][i]);
						}
					}
				}
			}
		}
	}
	else
	{
		int enemy_pos;
		int my_pos,my_pos2;
		int my_x,my_x2;
		int my_y,my_y2;
		int enemy_x;
		int enemy_y;
		int Manhattan = 100,Manhattan2 = 100;
		int test = 0;
		int count = 0;
		for(POS p=0;p<32;p++)
		{
			test = 0;
			count = 0;
			int position1=0,position2=0;
			if(GetColor(B.fin[p]) == B.who^1 && B.fin[p] < 14)
			{
				enemy_pos = p;
				//cout << "敵方位置:" << p << ",子力:" << GetLevel(B.fin[enemy_pos]) << endl; 
				enemy_x = enemy_pos/4;
				enemy_y = enemy_pos%4;
				for(POS t=0;t<32;t++)
				{
					if(GetColor(B.fin[t]) == B.who && GetLevel(B.fin[t]) != LVL_C && B.fin[t] < 14 && ChkEats(B.fin[t],B.fin[enemy_pos]) && !ChkEats(B.fin[enemy_pos],B.fin[t]) && position1 == 0)
					{
						//cout << "第一個:" << B.fin[t] << ",位置:" << t << endl;
						my_pos = t;
						test = 1;
						position1 = 1;
						if(position2 == 1)
						{
							break;
						}
						else
							continue;
					}
					if(GetColor(B.fin[t]) == B.who && GetLevel(B.fin[t]) != LVL_C && B.fin[t] < 14 && ChkEats(B.fin[t],B.fin[enemy_pos]) && position2 == 0)
					{
						//cout << "第二個:" << B.fin[t] << ",位置:" << t << endl;
						my_pos2 = t;
						test = 1;
						position2 = 1;
						if(position1 == 1)
						{
							break;
						}
						else
							continue;
					}
				}
			}
			if(test == 1)
			{
				break;
			}
		}
		if(test == 0)
		{
			//cout << "開始跑步" << endl; 
			return run(B);
		}
		
		MOVLST lst;
		my_x = my_pos/4;
		my_y = my_pos%4;
		my_x2 = my_pos2/4;
		my_y2 = my_pos2%4;
		enemy_x = enemy_pos/4;
		enemy_y = enemy_pos%4;
		Manhattan = abs(my_x,enemy_x)+abs(my_y,enemy_y);
		Manhattan2 = abs(my_x2,enemy_x)+abs(my_y2,enemy_y);
		//cout << "Manhattan distance=" << Manhattan << endl;
		//cout << "Manhattan distance2=" << Manhattan2 << endl;
		if(Manhattan == 1)
		{
			//cout << "111 kill" << endl; 
			return MOV(my_pos,enemy_pos);
		}
		else if(Manhattan2 == 1)
		{
			//cout << "222 kill" << endl;
			return MOV(my_pos2,enemy_pos);
		}
		if(abs(my_x,enemy_x) == 1 && abs(my_y,enemy_y) == 1 && abs(my_x2,enemy_x) == 1 && abs(my_y2,enemy_y) == 1)
		{
			if(ChkEats(B.fin[my_pos],B.fin[enemy_pos]) && !ChkEats(B.fin[enemy_pos],B.fin[my_pos]))
			{
				MOVLST lst;
				B.MoveGen(lst);
				for(int i=0;i<lst.num;i++)
				{
					if(lst.mov[i].st == my_pos && lst.mov[i].state == 2)
					{
						return lst.mov[i];
					}
				}
			}
			if(ChkEats(B.fin[my_pos2],B.fin[enemy_pos]) && !ChkEats(B.fin[enemy_pos],B.fin[my_pos2]))
			{
				MOVLST lst;
				B.MoveGen(lst);
				for(int i=0;i<lst.num;i++)
				{
					if(lst.mov[i].st == my_pos2 && lst.mov[i].state == 2)
					{
						return lst.mov[i];
					}
				}
			}
		}
		if(Manhattan == 2 && Manhattan2 == 2)
		{
			if(ChkEats(B.fin[my_pos],B.fin[enemy_pos]) && !ChkEats(B.fin[enemy_pos],B.fin[my_pos]))
			{
				MOVLST lst;
				B.MoveGen(lst);
				for(int i=0;i<lst.num;i++)
				{
					if(lst.mov[i].st == my_pos && lst.mov[i].state == 2)
					{
						return lst.mov[i];
					}
				}
			}
			if(ChkEats(B.fin[my_pos2],B.fin[enemy_pos]) && !ChkEats(B.fin[enemy_pos],B.fin[my_pos2]))
			{
				MOVLST lst;
				B.MoveGen(lst);
				for(int i=0;i<lst.num;i++)
				{
					if(lst.mov[i].st == my_pos2 && lst.mov[i].state == 2)
					{
						return lst.mov[i];
					}
				}
			}
		}
		if(Manhattan < Manhattan2 && !(abs(my_x2,enemy_x) == 1 && abs(my_y2,enemy_y) == 1))
		{
			//cout << "交換位置" << endl; 
			int temp;
			temp = my_pos;
			my_pos = my_pos2;
			my_pos2 = temp; 
		}
		else if(Manhattan == Manhattan2 && abs(my_x,enemy_x) == 1 && abs(my_y,enemy_y) == 1)
		{
			//cout << "交換位置" << endl; 
			int temp;
			temp = my_pos;
			my_pos = my_pos2;
			my_pos2 = temp; 
		}
		my_x = my_pos/4;
		my_y = my_pos%4;
		my_x2 = my_pos2/4;
		my_y2 = my_pos2%4;
		Manhattan = abs(my_x,enemy_x)+abs(my_y,enemy_y);
		Manhattan2 = abs(my_x2,enemy_x)+abs(my_y2,enemy_y);
		if(Manhattan == 1)
		{
			//cout << "必殺吃子步:" << my_pos << "->" << enemy_pos << endl;
			return MOV(my_pos,enemy_pos);
		}
		else
		{
			if(abs(my_x,enemy_x) == 1 && abs(my_y,enemy_y) == 1)
			{
			}
			else
			{
				if(abs(my_x,enemy_x) > abs(my_y,enemy_y))
				{
					for(int i=3;i>=0;i--)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							enemy_x = enemy_pos/4;
							enemy_y = enemy_pos%4;
							my_x = ADJ[my_pos][i]/4;
							my_y = ADJ[my_pos][i]%4;
							if(Manhattan >= abs(my_x,enemy_x)+abs(my_y,enemy_y))
							{
								//cout << "必殺步:" << my_pos << "->" << ADJ[my_pos][i] << endl;
								return MOV(my_pos,ADJ[my_pos][i]);
							}
						}
					}
					if(my_x > enemy_x)
					{
						my_pos -= 4;
					}
					else
					{
						my_pos += 4;
					}
					for(int i=0;i<4;i++)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							//cout << "移開棋子" << my_pos << "->" << ADJ[my_pos][i] << endl;
							return MOV(my_pos,ADJ[my_pos][i]);
						}
					}
					
				}
				else
				{
					for(int i=0;i<4;i++)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							enemy_x = enemy_pos/4;
							enemy_y = enemy_pos%4;
							my_x = ADJ[my_pos][i]/4;
							my_y = ADJ[my_pos][i]%4;
							if(Manhattan >= abs(my_x,enemy_x)+abs(my_y,enemy_y))
							{
								//cout << "必殺步:" << my_pos << "->" << ADJ[my_pos][i] << endl;
								return MOV(my_pos,ADJ[my_pos][i]);
							}
						}
					}
					if(my_y > enemy_y)
					{
						my_pos -= 1;
					}
					else
					{
						my_pos += 1;
					}
					for(int i=0;i<4;i++)
					{
						if(ADJ[my_pos][i] != -1 && B.fin[ADJ[my_pos][i]] == 15)
						{
							//cout << "移開棋子" << my_pos << "->" << ADJ[my_pos][i] << endl;
							return MOV(my_pos,ADJ[my_pos][i]);
						}
					}
				}
			}
		}
	}
}

MOV Play(const BOARD &B) {
#ifdef _WINDOWS
	Tick=GetTickCount();
	TimeOut = (DEFAULTTIME-3)*1000;
#else
	Tick=clock();
	TimeOut = (DEFAULTTIME-3)*CLOCKS_PER_SEC;
#endif
	POS p; 
	int c=0;
	
	//cout << "我方剩餘時間:" << remain_time << endl;
	
	/*cout << "\n剩餘棋子: "; 
	for(int i=0;i<14;i++)
	{
		cout << B.overage[i] << " ";
	}
	cout << endl;*/
	
	// 新遊戲,隨機翻子
	if(B.who==-1)
	{
		int initial[12] = {5,6,9,10,13,14,17,18,21,22,25,26};
		p = rand()%12;
		//printf("%d\n",p);
		return MOV(initial[p],initial[p]);
	}
	
	clock_t start = clock();
	clock_t end = start + 20*CLOCKS_PER_SEC;
	int best_move = -INF;
	int depth = 1;
	int best = 0;
	MOVLST lst;
	clock_t clock_pre,clock_now;
	clock_pre = clock();
	
	int enemy_count = 0;
	int enemy_level;
	for(int i=0;i<14;i++)
	{
		if(i/7 == B.who^1)
		{
			enemy_count += B.overage[i];
			if(enemy_count > 2)
			{
				break;
			}
			enemy_level = i;
		}
	}
	if(enemy_count < 3)
	{
		return absolute_eat_move(B,enemy_count);
	}
	
	//現在雜湊值 
	unsigned long long int ZobristKey = 0; 
	for(POS p=0;p<32;p++)
	{
		if(B.fin[p] < 14)
		{
			//cout << p << ": " << B.fin[p] << endl;
			ZobristKey ^= Zobrist[B.fin[p]][p];
		}
	}
	//ZobristKey ^= ZobristPlayer;
	//cout << "ZobristKey=" << ZobristKey << endl;
	
	//History heuristic 
	
	for (int i = 0; i<32; i++)
	{
		for (int j = 0; j<32; j++)
		{
			//cout << HT[i][j] << " ";
			HT[i][j] /= 2;
		}
		//cout << endl;
	}
	
	int test = B.MoveGen(lst);
	
	/*cout << "走法: " << endl;
	for(int i=0;i<lst.num;i++)
	{
		cout << i+1 << ". " << lst.mov[i].st << "->" << lst.mov[i].ed << " ,分數:" << HT[lst.mov[i].st][lst.mov[i].ed] << " ,狀態:" << lst.mov[i].state << endl;
	}
	cout << endl;*/
	
	int threshold = 25;
	bool cnt_condition = 0;
	for(int i=0;i<14;i++)
	{
		if(B.cnt[i] != 0)
		{
			cnt_condition = 1;
			break;
		}
	}
	if(cnt_condition)
		BestMove = BestMove_temp = MOV(-1,-1);
	else
		BestMove = BestMove_temp = lst.mov[0];
	
	while(test > 1 || (test == 1 && lst.mov[0].st != -1 && lst.mov[0].ed != -1))
	{
		/*if(depth > 4)
			break;*/
		clock_now = clock();
		if( clock_now > end || depth > 50 || clock_now-clock_pre > end-clock_now )
			break;
		else
		{
			eat_move = 0;
			pass = 0;
			//動態調整搜尋區間大小 
			int temp = SearchMax(B,best_move-threshold,best_move+threshold,ZobristKey,0,depth);
			if(temp <= best_move-threshold) //failed-low
				temp = SearchMax(B,-INF,temp,ZobristKey,0,depth);
			else if(temp >= best_move+threshold) //failed-high
				temp = SearchMax(B,temp,INF,ZobristKey,0,depth);
			best_move = temp;
			BestMove = BestMove_temp;
			BestMove.state = BestMove_temp.state;
			
			/*best = SearchMax(B,-INF,INF,ZobristKey,0,depth);
			//cout << "路徑: " << BestMove_temp.st << "->" << BestMove_temp.ed << endl;
			best_move = best;
			BestMove = BestMove_temp;
			BestMove.state = BestMove_temp.state;*/
		}
		clock_pre = clock_now;
		//cout << endl << "深度:" << depth << ",best=" << best_move << ",BestMove= " << BestMove.st << "->" << BestMove.ed << endl << endl;
		depth++;
	}
	
	BOARD N(B);
	N.Move(BestMove);
	//cout << "原盤面分數:" << Eval(B) << ",更改後盤面分數:" << -Eval(N) << endl;
	
	// 若搜出來的結果會比現在好就用搜出來的走法
	if(cnt_condition == 1)
	{
		BOARD PASS(B);
		PASS.Move(MOV(-1,-1));
		int flip = SearchMin(PASS,-INF,INF,ZobristKey^ZobristPlayer,0,depth-1);
		//cout <<  "原最佳分數" << best_move << ",走法:" << BestMove.st << "->" << BestMove.ed << ",狀態:" << BestMove.state << endl;
		//cout << "翻子分數:" << flip << endl;
		if(flip > best_move || (flip == best_move && BestMove.state == 0) || (BestMove.st == -1 && BestMove.ed == -1))// 翻棋 
		{
			p = Flip_where(B,ZobristKey);
			//cout << "翻子:" << p << endl;
			return MOV(p,p);
		}
		else
		{
			BOARD N(B);
			N.Move(BestMove);
			ZobristKey = 0;
			for(POS p=0;p<32;p++)
			{
				if(N.fin[p] < 14)
				{
					ZobristKey ^= Zobrist[N.fin[p]][p];
				}
			}
			ZobristKey ^= ZobristPlayer;
			Game_record[Game_length++] = ZobristKey;
			/*cout << endl << "目前為止盤面:" << endl;
			for(int i=0;i<Game_length;i++)
			{
				cout << i+1 << ", " << Game_record[i] << endl ;
			}
			cout << "路徑: " << BestMove.st << "->" << BestMove.ed << endl;*/
			return BestMove;
		}
	}
	else
	{
		if(best_move == Eval(B))
		{
			int test = 0;
			for(int j=0;j<lst.num;j++)
			{
				if(lst.mov[j].state == 1)
				{
					BestMove = lst.mov[j];
					return BestMove;
				}
			}
			for(int j=0;j<lst.num;j++)
			{
				if(lst.mov[j].state == 2)
				{
					BestMove = lst.mov[j];
					return BestMove;
				}
			}
		}
		else if(best_move == -WIN) //必輸情況,盡量跑
		{
			//cout << "盡量跑" << endl;
			return run(B);
		}
		else if(best_move == WIN)
		{
			//cout << "有必勝步！" << endl; 
			BOARD N(B);
			N.Move(BestMove);
			ZobristKey = 0;
			for(POS p=0;p<32;p++)
			{
				if(N.fin[p] < 14)
				{
					ZobristKey ^= Zobrist[N.fin[p]][p];
				}
			}
			ZobristKey ^= ZobristPlayer;
			Game_record[Game_length++] = ZobristKey;
			return BestMove;
		}
		else
		{
		}
		
		BOARD N(B);
		N.Move(BestMove);
		ZobristKey = 0;
		for(POS p=0;p<32;p++)
		{
			if(N.fin[p] < 14)
			{
				ZobristKey ^= Zobrist[N.fin[p]][p];
			}
		}
		ZobristKey ^= ZobristPlayer;
		/*for(int i=0;i<Game_length;i++)
		{
			if(Game_record[i] == ZobristKey)
			{
				for(int j=0;j<lst.num;j++)
				{
					if(lst.mov[i].state == 1)
					{
						BestMove = lst.mov[i];
						return BestMove;
					}
				}
				for(int j=0;j<lst.num;j++)
				{
					if(lst.mov[i].state == 2)
					{
						BestMove = lst.mov[i];
						return BestMove;
					}
				}
			}
		}*/
		Game_record[Game_length++] = ZobristKey;
		/*cout << endl << "目前為止盤面:" << endl;
		for(int i=0;i<Game_length;i++)
		{
			cout << i+1 << ", " << Game_record[i] << endl;
		}
		cout << "路徑: " << BestMove.st << "->" << BestMove.ed << endl;*/
		return BestMove;
		
	}

}

FIN type2fin(int type) {
    switch(type) {
	case  1: return FIN_K;
	case  2: return FIN_G;
	case  3: return FIN_M;
	case  4: return FIN_R;
	case  5: return FIN_N;
	case  6: return FIN_C;
	case  7: return FIN_P;
	case  9: return FIN_k;
	case 10: return FIN_g;
	case 11: return FIN_m;
	case 12: return FIN_r;
	case 13: return FIN_n;
	case 14: return FIN_c;
	case 15: return FIN_p;
	default: return FIN_E;
    }
}
FIN chess2fin(char chess) {
    switch (chess) {
	case 'K': return FIN_K;
	case 'G': return FIN_G;
	case 'M': return FIN_M;
	case 'R': return FIN_R;
	case 'N': return FIN_N;
	case 'C': return FIN_C;
	case 'P': return FIN_P;
	case 'k': return FIN_k;
	case 'g': return FIN_g;
	case 'm': return FIN_m;
	case 'r': return FIN_r;
	case 'n': return FIN_n;
	case 'c': return FIN_c;
	case 'p': return FIN_p;
	default: return FIN_E;
    }
}

// Generates a Randome number from 0 to 2^32-1
unsigned long long int randomInt()
{
    uniform_int_distribution<unsigned long long int>dist(0, UINT64_MAX);
    return dist(mt);
}

void Initial_Zobrist()
{
	for (int i = 0; i<14; i++)
      for (int j = 0; j<32; j++)
          Zobrist[i][j] = randomInt();
}

void Initial_HT()
{
	for (int i = 0; i<32; i++)
      for (int j = 0; j<32; j++)
          HT[i][j] = 0;
}

int main(int argc, char* argv[]) {

#ifdef _WINDOWS
	srand(Tick=GetTickCount());
#else
	srand(Tick=time(NULL));
#endif
	BOARD B;
	if (argc<=1) {
	    TimeOut=(B.LoadGame("board.txt")-3)*1000;
	    if(!B.ChkLose())
			Output(Play(B));
	    return 0;
	}
	Protocol *protocol;
	protocol = new Protocol();
	protocol->init_protocol(argv[argc-2],atoi(argv[argc-1]));
	int iPieceCount[14];
	char iCurrentPosition[32];
	int type;
	bool turn;
	PROTO_CLR color;

	char src[3], dst[3], mov[6];
	History moveRecord;
	protocol->init_board(iPieceCount, iCurrentPosition, moveRecord, remain_time);
	protocol->get_turn(turn,color);

	TimeOut = (DEFAULTTIME-3)*1000;
	
	
	Initial_Zobrist();
	Initial_HT();
	/*for (int i = 0; i<14; i++)
	{
		for (int j = 0; j<32; j++)
		{
			cout << Zobrist[i][j] << " ";
		}
		cout << endl;
	}*/
          
	B.Init(iCurrentPosition, iPieceCount, (color==2)?(-1):(int)color);
	
	MOV m;
	if(turn) // 我先
	{
		cout << "turn=" << turn << endl;
	    m = Play(B);
	    sprintf(src, "%c%c",(m.st%4)+'a', m.st/4+'1');
	    sprintf(dst, "%c%c",(m.ed%4)+'a', m.ed/4+'1');
	    protocol->send(src, dst);
	    protocol->recv(mov, remain_time);
	    if( color == 2)
		color = protocol->get_color(mov);
	    B.who = color;
	    B.DoMove(m, chess2fin(mov[3]));
	    protocol->recv(mov, remain_time);
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	}
	else // 對方先
	{
		cout << "turn=" << turn << endl;
	    protocol->recv(mov, remain_time);
	    if( color == 2)
	    {
		color = protocol->get_color(mov);
		B.who = color;
	    }
	    else {
		B.who = color;
		B.who^=1;
	    }
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	}
	B.Display();
	while(1)
	{
	    m = Play(B);
	    sprintf(src, "%c%c",(m.st%4)+'a', m.st/4+'1');
	    sprintf(dst, "%c%c",(m.ed%4)+'a', m.ed/4+'1');
	    protocol->send(src, dst);
	    protocol->recv(mov, remain_time);
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	    B.Display();
	    protocol->recv(mov, remain_time);
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	    B.Display();
	}

	return 0;
}
