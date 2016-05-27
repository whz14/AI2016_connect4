#include <iostream>
#include <fstream>
#include <Windows.h>
#include <cassert>
#include <cmath>
//#include <conio.h>
//#include <atlstr.h>
#include <ctime>
#include "Point.h"
#include "Judge.h"
#include "Strategy.h"

#define _cprintf //_cprintf
#define MACHINE true
#define USR false
const double INF = 1e+6;
const int TTLSIZE = 11000000;
using namespace std;

int** outBoard = NULL;
int outTop[20];
bool flag = false;

struct tNode {
	static int** board;
	static int top[20];
	static int m, n;
	static int next;

	int parent;
	int children[12];
	int childNum;
	double won;
	int visited;
	int posi;	// only column.
	bool usrOrMachine;	// 0 for usr, 1 for mechine
	bool expandable;

	tNode(): parent(-1), childNum(0), won(0), visited(0), posi(-1), expandable(1){}
	static int init(const int* _board, const int* _top, const int M, const int N, const int NoX, const int NoY, int lastY);
	//init() return the time it took.

	inline double UCB(double c);
	inline void playAt();
	inline void dePlay();
	double randPlay();	// and return a result
	void destroy();	// recursively
	void destroyItself();
	void newChild(int par, int pos, bool uOrM);	// 父节点parent, 落子位置position, 用户还是机器userOrMachine

	inline int treePolicy();
	pair<double, int> defaultPolicy();
	int bstChild(double c);
	void backUp(double d, int visPlus);
	int UCTSearch(unsigned int timeLimit);// 参数单位:ms

	friend ostream& operator << (ostream& out, tNode& node);
	void print();

};

int** tNode::board = NULL;
int tNode::top[20] = {0};
int tNode::m = 0, tNode::n = 0;
int tNode::next = 1;
tNode UCTree[TTLSIZE];
bool occupied[TTLSIZE] = {false};
int totalNodes = 0;

void destroyBut(int posiLeft) {
/*
 *除落子在posiLeft的儿子之外的所有儿子均删掉，并把树的根节点变成这个儿子。
 */
	if(!UCTree[0].childNum) {
		_cprintf("root has no children?\n");
		return;
	}
	DWORD s_time = GetTickCount();
	int chiLeft = -1;
	for(int i = 0; i < UCTree[0].childNum; ++i) {
		if(posiLeft == UCTree[UCTree[0].children[i]].posi) {
			chiLeft = UCTree[0].children[i];	// 这个儿子的下标就是chiLeft
			continue;
		}
		UCTree[UCTree[0].children[i]].destroy();
	}
	UCTree[0] = UCTree[chiLeft];
	UCTree[0].parent = -1;
	UCTree[0].posi = -1;
	for(int i = 0; i < UCTree[0].childNum; ++i) {
		UCTree[UCTree[0].children[i]].parent = 0;
	}
	UCTree[chiLeft].destroyItself();
	_cprintf("destroyAllBut() took %dms\n", GetTickCount() - s_time);
}


/*
	策略函数接口,该函数被对抗平台调用,每次传入当前状态,要求输出你的落子点,
	该落子点必须是一个符合游戏规则的落子点,不然对抗平台会直接认为你的程序有误

	input:
		为了防止对对抗平台维护的数据造成更改，所有传入的参数均为const属性
		M, N : 棋盘大小 M - 行数 N - 列数 均从0开始计， 左上角为坐标原点，行用x标记，列用y标记
		top : 当前棋盘每一列列顶的实际位置. e.g. 第i列为空,则_top[i] == M, 第i列已满,则_top[i] == 0
		_board : 棋盘的一维数组表示, 为了方便使用，在该函数刚开始处，我们已经将其转化为了二维数组board
			你只需直接使用board即可，左上角为坐标原点，数组从[0][0]开始计(不是[1][1])
			board[x][y]表示第x行、第y列的点(从0开始计)
			board[x][y] == 0/1/2 分别对应(x,y)处 无落子/有用户的子/有程序的子,不可落子点处的值也为0
		lastX, lastY : 对方上一次落子的位置, 你可能不需要该参数，也可能需要的不仅仅是对方一步的
			落子位置，这时你可以在自己的程序中记录对方连续多步的落子位置，这完全取决于你自己的策略
		noX, noY : 棋盘上的不可落子点(注:其实这里给出的top已经替你处理了不可落子点，也就是说如果某一步
			所落的子的上面恰是不可落子点，那么UI工程中的代码就已经将该列的top值又进行了一次减一操作，
			所以在你的代码中也可以根本不使用noX和noY这两个参数，完全认为top数组就是当前每列的顶部即可,
			当然如果你想使用lastX,lastY参数，有可能就要同时考虑noX和noY了)
		以上参数实际上包含了当前状态(M N _top _board)以及历史信息(lastX lastY),你要做的就是在这些信息下给出尽可能明智的落子点
	output:
		你的落子点Point
*/
extern "C" __declspec(dllexport) Point* getPoint(const int M, const int N, const int* top, const int* _board,
	const int lastX, const int lastY, const int noX, const int noY){
	/*
		不要更改这段代码
	*/
	//AllocConsole();
	_cprintf("\n\nflag is %d, Y is %d\n", flag, lastY);
	if(!flag) {
		_cprintf("rand seed planted\n");
		srand(static_cast<unsigned int>(time(NULL)));
	}
	else {
		_cprintf("root.usrOrMachine is %d\n", UCTree[0].usrOrMachine);
	}

	int timeTook = tNode::init(_board, top, M, N, noX, noY, lastY);
	UCTree[0].usrOrMachine = USR;	// USR actually means my rival. MACHINE is my AI!!!!!!!!!
	occupied[0] = true;
	// initialized
	
	int res = UCTree[0].UCTSearch(5000 - 400 - timeTook);	// 400ms for the next destroction
	int y = UCTree[res].posi;
	int x = top[y] - 1;
	// search finished, 
	
	for(int i = 0; i < M; ++i) {	// copy the board.
		for(int j = 0; j < N; ++j) {
			_cprintf("%d ", _board[i * N + j]);
		}
		_cprintf("\n");
	}
	for(int i = 0; i < N; ++i) {
		_cprintf("%d ", top[i]);
	}
	_cprintf("and point chosen is (%d, %d)\n", x, y);

	_cprintf("\nroot.visited is %d and ttlnodes is %d\n", UCTree[0].visited, totalNodes);
	_cprintf("%d nodes expanded", tNode::next);
	_cprintf("the UCB are\n");
	for(int i = 0; i < UCTree[0].childNum; ++i) {
		_cprintf("%d\t %lf/%d %lf\n", i, UCTree[UCTree[0].children[i]].won, UCTree[UCTree[0].children[i]].visited, UCTree[UCTree[0].children[i]].UCB(0));
	}

	destroyBut(UCTree[res].posi);

	return new Point(x, y);
}

/*
	getPoint函数返回的Point指针是在本dll模块中声明的，为避免产生堆错误，应在外部调用本dll中的
	函数来释放空间，而不应该在外部直接delete
*/
extern "C" __declspec(dllexport) void clearPoint(Point* p){
	delete p;
	return;
}
/*
	清除top和board数组
*/

void clearArray(int M, int N, int** board){
	for(int i = 0; i < M; i++){
		delete[] board[i];
	}
	delete[] board;
}

bool AlreadyStarted(const int* board, int m, int n) {
/**
 * 检查棋盘判断是中盘还是开局。如果有多于一个字，那么就是中盘，否则开局。
 */
	for(int i = 0; i < m; ++i) {	// copy the board.
		for(int j = 0; j < n; ++j) {
			_cprintf("%d ", board[i * n + j]);
		}
		_cprintf("\n");
	}

	int ttl = 0;
	for(int i = 0; i < m; ++i) {
		for(int j = 0; j < n; ++j) {
			ttl += (int)(board[i*n + j] > 0);
		}
	}
	_cprintf("ttl is %d\n", ttl);
	return ttl > 1;
}

int tNode::init(const int * _board, const int* _top, 
	const int M, const int N, const int NoX, const int NoY, int lastY) {
	DWORD s_t = GetTickCount();
	//首先计时，以便之后搜索调整时间。

	flag = AlreadyStarted(_board, m, n);
	_cprintf("started? %d\n", flag);
	next = 1;
	//从头扩展这棵树。
	if(!flag) {	// 初次开局，new出指针。否则不new以节省时间
		m = M, n = N;
		if(board != NULL) {
			clearArray(m, n, board);
			clearArray(m, n, outBoard);
		}
		board = new int*[M];
		outBoard = new int*[M];
		for(int i = 0; i < M; ++i) {
			board[i] = new int[N];
			outBoard[i] = new int[N];
		}
		flag = true;
		UCTree[0].destroy();
	}
	else {
		destroyBut(lastY);	// 中盘，必有可用子树
	}

	for(int i = 0; i < M; ++i) {	// copy the board.
		for(int j = 0; j < N; ++j) {
			board[i][j] = _board[i * N + j];
		}
	}
	board[NoX][NoY] = -1;
	for(int i = 0; i < N; ++i) {	// copy the top
		top[i] = _top[i];
	}
	//cout << UCTree[0];
	int retVal = GetTickCount() - s_t;
	_cprintf("time of init is %d\n", retVal);
	return retVal;
}

inline double tNode::UCB(double c) {	// 某一节点的信心上界
	if(visited == 0) {
		return INF;
	}
	return won / visited + c * sqrt(2 * log(UCTree[parent].visited) / visited);
}

inline void tNode::playAt() {		// 某一节点落子
	board[top[posi] - 1][posi] = 1 + usrOrMachine;
	--top[posi];
	if(top[posi] && board[top[posi] - 1][posi]) {
		// assert(board[top[posi] - 1][posi] == -1);
		--top[posi];
	}
}

inline void tNode::dePlay() {		// 撤销落子
	if(posi == -1) {
		return;
	}
	if(board[top[posi]][posi] == -1) {
		++top[posi];
	}
	board[top[posi]++][posi] = 0;
}

double tNode::randPlay() {

	playAt();
	for(int i = 0; i < m; ++i) {
		for(int j = 0; j < n; ++j) {
			outBoard[i][j] = board[i][j];
		}
	}
	for(int i = 0; i < n; ++i) {
		outTop[i] = top[i];
	}
	dePlay();
	if(usrOrMachine) {	// machine's turn (of the random playing)
		if(machineWin(outTop[posi], posi, m, n, outBoard)) {	// my AI wins
			++visited;
			won += 1;
			expandable = false;
			assert(won == 1);
			return 1;
		}
	}
	else {	// usr's turn of the randPlay.
		if(userWin(outTop[posi], posi, m, n, outBoard)) {
			++visited;
			expandable = false;
			won += 1;
			assert(won == 1);
			return 1;
		}
	}
	if(isTie(n, outTop)) {	// played, but already tie.
		++visited;
		expandable = false;
		return 0;
	}

	for(int i = 1; ; ++i) {
		int x = rand() % n;
		if(!outTop[x]) {
			--i;
			continue;
		}
		outBoard[--outTop[x]][x] = ((i + (int)usrOrMachine) & 1) + 1;	// (i + (int)usrOrMachine)&1 <=> machine's turn

		if((i + (int)usrOrMachine) & 1) {	// machine's turn (of the random playing)
			if(machineWin(outTop[x], x, m, n, outBoard)) {
				++visited;	// now that there's a result
				if(usrOrMachine) {	// machine's turn (of the determined tree) and machine won. so praise.
					won += 1;
					return 1;
				}
				else {	// usr's turn(of the determined tree), while machine won the random playing
					won -= 1;
					return -1;
				}
			}
		}
		else {	// usr's turn of the randPlay.
			if(userWin(outTop[x], x, m, n, outBoard)) {
				++visited;
				if(usrOrMachine) {	// machine's turn in the search tree, but usr won. so punish.
					won -= 1;
					return -1;
				}
				else {
					won += 1;
					return 1;
				}
			}
		}

		if(outTop[x] && outBoard[outTop[x] - 1][x]) {
			assert(outBoard[outTop[x] - 1][x] == -1);
			--outTop[x];
		}

		if(isTie(n, outTop)) {	// played, but already tie.
			++visited;
			return 0;
		}
	}
	
	return 0.0;
}

void tNode::destroy() {		// 递归删除所有子树
	for(int i = 0; i < childNum; ++i) {
		UCTree[children[i]].destroy();
	}
	destroyItself();
}

void tNode::destroyItself() {
	--totalNodes;
	occupied[this - UCTree] = false;
	parent = -1;
	memset(children, 0, sizeof(children));
	childNum = 0;
	won = 0;
	visited = 0;
	posi = -1;
	expandable = true;
}

void tNode::newChild(int par, int pos, bool uOrM) {
// 【本节点】的子节点
	while(occupied[next]) {
		++next;
	}
	occupied[next] = true;
	++totalNodes;
	children[childNum++] = next;
	UCTree[next].parent = par;
	UCTree[next].posi = pos;
	UCTree[next].usrOrMachine = uOrM;
}

int tNode::bstChild(double c) {
// UCT最大的子节点
	assert(childNum);
	int ans = children[0];
	double max = UCTree[ans].UCB(c);
	if(max == INF) {
		return ans;
	}
	for(int i = 1; i < childNum; ++i) {
		double tmpmax = UCTree[children[i]].UCB(c);
		if(tmpmax == INF) {
			return children[i];
		}
		else if(tmpmax > max) {
			max = tmpmax;
			ans = children[i];
		}
	}
	return ans;
}

int tNode::UCTSearch(unsigned int limit) {
	DWORD s_time = GetTickCount();
	for(int times = 0; GetTickCount() - s_time < limit; ++times) {
		int v = UCTree[0].treePolicy();
		auto delta = UCTree[v].defaultPolicy();	// pair<double, int>
		UCTree[v].backUp(delta.first, delta.second);
	}
	return UCTree[0].bstChild(0);
}

inline int tNode::treePolicy() {
	int ans = 0;
	while(UCTree[ans].childNum) {
		ans = UCTree[ans].bstChild(0.8);
		UCTree[ans].playAt();	// 更新棋盘
	}
	return ans;
}

pair<double, int> tNode::defaultPolicy() {
// 注意：扩展所有子节点以对当前局面有更好评估
	assert(childNum == 0);
	if(!expandable) {	// visited leaf. must be a state already has result.
		return make_pair(won/abs(won), 1);
	}
	double ans = 0;
	for(int p1 = 0; p1 < n; ++p1) {	// expand all the children
		if(!top[p1]) {
			continue;
		}
		newChild(this - UCTree, p1, !usrOrMachine);
	}
	for(int i = 0; i < childNum; ++i) {
		ans += UCTree[children[i]].randPlay();	// randPlay() will also handle with the result-determined state.
	}
	return make_pair(-ans, childNum);	// attention to the '-'. ans is the total result of the children, so for itself, the result is -ans
}

void tNode::backUp(double d, int visPlus) {
	for(int v = this - UCTree; v >= 0; v = UCTree[v].parent) {
		UCTree[v].dePlay();	//还原棋盘
		UCTree[v].won += d;
		d *= -1;
		UCTree[v].visited += visPlus;
	}
}

ostream & operator<<(ostream& out, tNode& node) {
	out << &node - UCTree << "th node==========\n";
	out << "won " << node.won << " times out of " << node.visited << " times\n";
	if(&node - UCTree)
		out << "UCB is " << node.UCB(0) << endl;
	out << "usrOrMah\t" << node.usrOrMachine << "\tplayed@ " << node.posi << endl;
	out << node.childNum << " children" << "\tparent\t" << node.parent << "\n\n";

	int tmp[100] = {0};
	int ne = 0;
	for(int p = &node - UCTree; p > 0; p = UCTree[p].parent) {
		tmp[ne++] = p;
	}
	for(int i = ne - 1; i >= 0; --i) {
		UCTree[tmp[i]].playAt();
	}
	for(int i = 0; i < node.m; ++i) {
		for(int j = 0; j < node.n; ++j) {
			out << node.board[i][j] << ' ';
		}
		out << endl;
	}

	for(int p = &node - UCTree; p; p = UCTree[p].parent) {
		UCTree[p].dePlay();
	}
	out << "==============\n\n\n";
	return out;
}

void tNode::print() {
	_cprintf("%dth node=============\n", this - UCTree);
	_cprintf("won %lf times out of %d times\n", won, visited);
	if(this - UCTree)
		_cprintf("UCB is %lf\n", UCB(0));
	_cprintf("usrOrMah\t%d\tplayed@ %d\n", usrOrMachine, posi);
	_cprintf("%d children\tparent\t%d\n\n", childNum, parent);//; << node.childNum << " children" << "\tparent\t" << node.parent << "\n\n";

	int tmp[100] = {0};
	int ne = 0;
	for(int p = this - UCTree; p > 0; p = UCTree[p].parent) {
		tmp[ne++] = p;
	}
	for(int i = ne - 1; i >= 0; --i) {
		UCTree[tmp[i]].playAt();
	}
	for(int i = 0; i < m; ++i) {
		for(int j = 0; j < n; ++j) {
			_cprintf("%d ", board[i][j]);
		}
		_cprintf("\n");
	}

	for(int p = this - UCTree; p; p = UCTree[p].parent) {
		UCTree[p].dePlay();
	}
	_cprintf("==============\n\n\n");
}
