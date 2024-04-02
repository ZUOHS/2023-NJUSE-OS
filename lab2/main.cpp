#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdio>
#include <regex>

using namespace std;

extern "C"{
    void normalPrint(const char * cs, int len);
    void redPrint(const char * cs, int len);
}
void Print0(const string& output){
    const char * cs = output.c_str();
    normalPrint(cs, strlen(cs));
}
void Print1(const string& output){
    const char * cs = output.c_str();
    redPrint(cs, strlen(cs));
}

int BytesPerSec;    // 每扇区字节数
int SecPerClus;	    // 每簇扇区数
int RsvdSecCnt;	    // Boot记录占用的扇区数
int NumFATs;        // FAT表个数
int RootEntCnt;	    // 根目录最大文件数
int FATSz;          // 每个FAT表所占扇区数
int BytesPerClus;   // 每簇字节数
int FATBase;        // FAT区域初始位置的偏移量
int FileRootBase;   // 文件根目录初始位置偏移量
int DataBase;       // 数据区初始位置偏移量






#pragma pack(push)
#pragma pack(1)
//引导扇区
class Fat12Header {
    unsigned short BPB_BytesPerSec; // 每扇区字节数
    unsigned char BPB_SecPerClus;   // 每簇占用的扇区数
    unsigned short BPB_RsvdSecCnt;  // Boot占用的扇区数
    unsigned char BPB_NumFATs;      // FAT表的记录数
    unsigned short BPB_RootEntCnt;  // 最大根目录文件数
    unsigned short BPB_TotSec16;    // 每个FAT占用扇区数
    unsigned char BPB_Media;        // 媒体描述符
    unsigned short BPB_FATSz16;     // 每个FAT占用扇区数
    unsigned short BPB_SecPerTrk;   // 每个磁道扇区数
    unsigned short BPB_NumHeads;    // 磁头数
    unsigned int BPB_HiddSec;       // 隐藏扇区数
    unsigned int BPB_TotSec32;      // 如果BPB_TotSec16是0，则在这里记录

public:
    Fat12Header(FILE *myFile) {
        fseek(myFile, 11, SEEK_SET);
        //BPB starts from 11
        fread(this, 1, 25, myFile);
        //25,lenth of BPB
        BytesPerSec = BPB_BytesPerSec;
        SecPerClus = BPB_SecPerClus;
        RsvdSecCnt = BPB_RsvdSecCnt;
        NumFATs = BPB_NumFATs;
        RootEntCnt = BPB_RootEntCnt;
        FATSz = BPB_FATSz16 == 0 ? BPB_TotSec32 : BPB_FATSz16;
        BytesPerClus = SecPerClus * BytesPerSec;
        FATBase = RsvdSecCnt * BytesPerSec;
        FileRootBase = RsvdSecCnt * BytesPerSec + NumFATs * FATSz * BytesPerSec;
        DataBase = BytesPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytesPerSec - 1) / BytesPerSec);
    }
};
class Node {
    string name;
    string content;
    vector<Node *> children;
    Node * father;
    bool type;
    //true for File and false for Dir
    int firstClus;
    int size;
    string path;
    int directFile = 0;
    int directDir = 0;

public:
    Node() = default;
    Node(string name, bool type, int firstClus, int size): name(name), type(type), firstClus(firstClus), size(size) {}
    void setPath(string Path) {
        path = Path;
    }
    void setContent(string Content) {
        content = Content;
    }
    void addFile(Node* child) {
        children.push_back(child);
        directFile++;
    }
    void addDir(Node* child) {
        children.push_back(child);
        directDir++;
    }
    void setFather(Node* Father) {
        father = Father;
    }
    void setName(string Name) {
        name = Name;
    }
    int getFirstClus() const {
        return firstClus;
    }
    int getSize() const {
        return size;
    }

    string getPath() {
        return path;
    }

    string getName() {
        return name;
    }

    Node * getFather() {
        return father;
    }

    vector<Node *> getChildren() {
        return children;
    }

    int getDirectFile() const {
        return directFile;
    }

    int getDirectDir() const {
        return directDir;
    }

    string getContent() {
        return content;
    }

    void setType(bool isFile) {
        type = isFile;
    }

    bool getType() const {
        return type;
    }

    Node * findChild(string pathz) {
        for (auto & i : children) {
            if (i->getName() == pathz) {
                return i;
            }
        }
        return nullptr;
    }
};


int findFat(FILE * myFile, int count);
string RealName(unsigned char * DIR_Name, bool type);
string getFileContent(FILE * myFile, int start);
void getDirLists(FILE * myFile, Node * toAdd);
vector<string> split(string input, char div);
Node * findPath(string input);
void dealLS(Node * toDeal);
void dealLSL(Node * toDeal);
void PrintLine(vector<Node *> children, bool withL);

Node * root = new Node();


class Entry {
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char reserve[10];
    unsigned short DIR_WrtTime;
    unsigned short DIR_WrtDate;
    unsigned short DIR_FstClus;
    unsigned int DIR_FileSize;
public:
    unsigned char * getDIR_Name() {
        return DIR_Name;
    }
    unsigned char getDIR_Attr() const {
        return DIR_Attr;
    }
    int getDIR_FstClus() const {
        return DIR_FstClus;
    }
    int getDIR_FileSize() const {
        return DIR_FileSize;
    }

    bool isValid() {

        if (DIR_Name[0] == '\0') {
            return false;
        }
        for (int i = 0; i < 11; i++) {
            if (!((DIR_Name[i] >= 'a' && DIR_Name[i] <= 'z') ||
                  (DIR_Name[i] >= 'A' && DIR_Name[i] <= 'Z') ||
                  (DIR_Name[i] >= '0' && DIR_Name[i] <= '9') ||
                  (DIR_Name[i] == ' '))) {
                break;
            }
            if (i == 10) {
                return true;
            }
        }

        return false;
    }

    void readRootEntry(FILE * myFile, const string& path) {
        root->setPath("/");
        root->setFather(root);
        root->setName("");
        root->setType(false);
        int offset = FileRootBase;
        for (int i = 0; i < RootEntCnt; i++, offset += 32) {
            fseek(myFile, offset, SEEK_SET);
            fread(this, 1, 32, myFile);

            if (isValid()) {
                if ((DIR_Attr & 0x10) == 0) {
                    Node * toAdd = new Node(RealName(DIR_Name, true), true, DIR_FstClus, DIR_FileSize);
                    toAdd->setFather(root);
                    toAdd->setPath("/");
                    root->addFile(toAdd);
                    if (DIR_FileSize != 0) {
                        toAdd->setContent(getFileContent(myFile, toAdd->getFirstClus()));
                    } else {
                        toAdd->setContent("");
                    }
//                    cout << "add "<< RealFileName(DIR_Name) << endl;
                } else if ((DIR_Attr & 0x20) == 0) {
                    Node * toAdd = new Node(RealName(DIR_Name, false), false, DIR_FstClus, DIR_FileSize);
                    toAdd->setFather(root);
                    toAdd->setPath("/");
                    root->addDir(toAdd);
                    getDirLists(myFile, toAdd);
//                    cout << "add "<< RealDirName(DIR_Name) << endl;
                }
            }
        }
    }
};
#pragma pack(pop)


int main() {
    FILE * myFile = fopen("./a.img", "rb");
    auto * myHeader = new Fat12Header(myFile);
    auto * start = new Entry;
    start->readRootEntry(myFile, "./a.img");

    string order;
    while (true) {
        Print0(">");
        getline(cin, order);
        if (order.empty()) {
            Print0("Input needed\n");
            continue;
        }
        vector<string> orders = split(order, ' ');
        if (orders[0] == "exit") {
            break;
        } else if (orders[0] == "cat") {
            if (orders.size() == 1) {
                Print0("Path needed\n");
            } else if (orders.size() > 2) {
                Print0("Too many paths/commands\n");
            } else {
                Node * toUse = findPath(orders[1]);
                if (toUse == nullptr) {
                    Print0("File don't exist\n");
                } else if (!toUse->getType()){
                    Print0("This is not a file\n");
                } else {
                    cout << toUse->getContent() << endl;
                }
            }
        } else if (orders[0] == "ls") {
            regex onlyLs ("ls[ ]*([0-9A-Z/.])+[ ]*");
            regex rootL ("ls([ ]+(-l+))+([ ])*");
            regex normalL1 ("ls[ ]+([A-Z0-9/.])+([ ]+(-l+))+[ ]*");
            regex normalL2 ("ls([ ]+(-l+))+[ ]+([A-Z0-9/.])+[ ]*");
            regex normalL3 ("ls([ ]+(-l+))+[ ]+([A-Z0-9/.])+([ ]+(-l+))+[ ]*");
            regex re ("(-l+)");
            if (orders.size() == 1) {
                dealLS(root);
            }  else if (regex_match(order, onlyLs)) {
                Node * toUse = findPath(orders[1]);
                if (toUse == nullptr) {
                    Print0("Path don't exist\n");
                } else if (toUse->getType()){
                    Print0("This is not a dir\n");
                } else {
                    dealLS(toUse);
                }
            } else if (regex_match(order, rootL)) {
                dealLSL(root);
            } else if (regex_match(order, normalL1) || regex_match(order, normalL2) || regex_match(order, normalL3)) {
                for (int i = 1; i < orders.size(); i++) {
                    if (orders[i][0] != '-') {
                        Node * toUse = findPath(orders[i]);
                        if (toUse == nullptr) {
                            Print0("Path don't exist\n");
                        } else if (toUse->getType()){
                            Print0("This is not a dir\n");
                        } else {
                            dealLSL(toUse);
                        }
                        break;
                    }
                }
            }  else {
                bool rightOrder = true;
                for (int i = 1; i < orders.size(); i++) {
                    if (orders[i][0] == '-' && !regex_match(orders[i], re)) {
                        rightOrder = false;
                    }
                }
                if (rightOrder) {
                    Print0("Path don't exist\n");
                } else {
                    Print0("Command don't exist\n");
                }
            }
        } else {
            Print0("Command don't exist\n");
        }
    }
    return 0;
}

string RealName(unsigned char * DIR_Name, bool type) {
    string result;
    int max = type? 8 : 11;
    for (int i = 0; i < max; i++) {
        if (DIR_Name[i] != ' ') {
            result += (char) DIR_Name[i];
        } else {
            break;
        }
    }
    if (type) {
        result += ".";
        for (int i = 8; i < 11; i++) {
            if (DIR_Name[i] != ' ') {
                result += (char) DIR_Name[i];
            } else {
                break;
            }
        }
    }
    return result;
}


string getFileContent(FILE * myFile, int start) {
    int currentClus = start;
    int nextClus = 0;
    string result;
    while (nextClus < 0xFF8) {
        nextClus = findFat(myFile, currentClus);
        char * temp = new char [512];
        fseek(myFile, DataBase + (currentClus - 2) * BytesPerClus, SEEK_SET);
        fread(temp, 1, BytesPerClus, myFile);
        result += temp;
        if (nextClus == 0xFF7) {
            Print0("Bad clus\n");
            break;
        }
        currentClus = nextClus;
    }
    return result;
}

int findFat(FILE * myFile, int count){
    int position = FATBase + count * 3 / 2;
    auto * temp = new unsigned short;
    fseek(myFile, position, SEEK_SET);
    fread(temp, 1, 2, myFile);
    return count % 2 == 0 ? (*temp & 0x0fff) : ((*temp) >> 4);
}

void getDirLists(FILE * myFile, Node * toAdd) {
    int currentClus = toAdd->getFirstClus();
    int nextClus = 0;
    while (nextClus < 0xFF8) {
        nextClus = findFat(myFile, currentClus);
        int offset = DataBase + (currentClus - 2) * BytesPerClus;
        if (nextClus == 0xFF7) {
            Print0("Bad clus\n");
            return;
        }
        for (int i = 0; i < 16; i++, offset += 32) {
            auto *thisEntry = new Entry();
            fseek(myFile, offset, SEEK_SET);
            fread(thisEntry, 1, 32, myFile);
            if (thisEntry->isValid()) {
                if ((thisEntry->getDIR_Attr() & 0x10) == 0) {
                    Node * toAdd2 = new Node(RealName(thisEntry->getDIR_Name(), true), true, thisEntry->getDIR_FstClus(), thisEntry->getDIR_FileSize());
                    toAdd2->setFather(toAdd);
                    toAdd2->setPath(toAdd->getPath() + toAdd->getName() + "/");
                    toAdd->addFile(toAdd2);
                    if (thisEntry->getDIR_FileSize() != 0) {
                        toAdd2->setContent(getFileContent(myFile, toAdd2->getFirstClus()));
                    } else {
                        toAdd2->setContent("");
                    }
//                    cout << "add "<< thisEntry->getDIR_Name() << endl;
                } else if ((thisEntry->getDIR_Attr() & 0x20) == 0) {
                    Node * toAdd2 = new Node(RealName(thisEntry->getDIR_Name(), false), false, thisEntry->getDIR_FstClus(), thisEntry->getDIR_FileSize());
                    toAdd2->setFather(toAdd);
                    toAdd2->setPath(toAdd->getPath() + toAdd->getName() + "/");
                    toAdd->addDir(toAdd2);
                    getDirLists(myFile, toAdd2);
//                    cout << "add "<< thisEntry->getDIR_Name() << endl;
                }
            }
        }
        currentClus = nextClus;
    }
}

//split the input by div
vector<string> split(string input, char div) {
    vector<string> result;
    string temp;
    for (int i = 0; i < input.length(); i++) {
        if (input[i] != div) {
            temp.push_back(input[i]);
        }
        if ((input[i] == div && !temp.empty()) || (i == input.length() - 1 && !temp.empty())) {
            result.push_back(temp);
            temp = "";
        }
    }
    return result;
}

Node * findPath(string input) {
    vector<string> paths = split(input, '/');
    Node * result = root;
    if (paths.empty()) {
        return result;
    }
    for (auto & path : paths) {
        if (path == "..") {
            result = result->getFather();
        } else if (path == ".") {
            result = result;
        } else {
            result = result->findChild(path);
            if (result == nullptr) {
                return result;
            }
        }
    }
    return result;
}

void dealLS(Node * toDeal) {
    if (toDeal == nullptr || toDeal->getType()) {
        return;
    }

    if (toDeal == root) {
        Print0("/:\n");
    } else {
        Print0(toDeal->getPath() + toDeal->getName() + "/:\n");
        Print1(".  ..  ");
    }

    vector<Node *> children = toDeal->getChildren();
    PrintLine(children, false);

    for (auto & i : children) {
        dealLS(i);
    }
}

void dealLSL(Node * toDeal) {
    if (toDeal == nullptr || toDeal->getType()) {
        return;
    }

    if (toDeal == root) {
        Print0("/ " + to_string(toDeal->getDirectDir()) + " " + to_string(toDeal->getDirectFile()) + ":\n");
    } else {
        Print0(toDeal->getPath() + toDeal->getName() + "/ " + to_string(toDeal->getDirectDir()) + " " + to_string(toDeal->getDirectFile()) + ":\n");
        Print1(".\n");
        Print1("..\n");
    }

    vector<Node *> children = toDeal->getChildren();
    PrintLine(children, true);

    for (auto & i : children) {
        dealLSL(i);
    }
}

void PrintLine(vector<Node *> children, bool withL) {
    for (auto & i : children) {
        if (!i->getType()) {
            Print1(i->getName() + "  ");
            if (withL) {
                Print0(to_string(i->getDirectDir()) + " " + to_string(i->getDirectFile()) + "\n");
            }
        } else {
            Print0(i->getName() + "  ");
            if (withL) {
                Print0(to_string(i->getSize()) + "\n");
            }
        }
    }
    Print0("\n");
}