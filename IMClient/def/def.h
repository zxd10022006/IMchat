#ifndef __DEF_H__
#define __DEF_H__

#define DEF_PROT_BASE 1000
#define DEF_PROT_REGISTER_RQ   (DEF_PROT_BASE+0)
#define DEF_PROT_REGISTER_RS   (DEF_PROT_BASE+1)
#define DEF_PROT_LOGIN_RQ      (DEF_PROT_BASE+2)
#define DEF_PROT_LOGIN_RS      (DEF_PROT_BASE+3)
#define DEF_PROT_FRIEND_INFO   (DEF_PROT_BASE+4)
#define DEF_PROT_CHAT_INFO_RQ  (DEF_PROT_BASE+5)
#define DEF_PROT_CHAT_INFO_RS  (DEF_PROT_BASE+6)
#define DEF_PROT_ADD_FRIEND_RQ (DEF_PROT_BASE+7)
#define DEF_PROT_ADD_FRIEND_RS (DEF_PROT_BASE+8)
#define DEF_PROT_FRIEND_OFFLINE (DEF_PROT_BASE+9)

#define REGIS_SUCC 0
#define REGIS_NICK_EXISTS 1
#define REGIS_TEL_EXISTS 2

#define LOGIN_SUCC 0    //成功
#define LOGIN_NOEX 1    //用户不存在
#define LOGIN_PASSERR 3 //密码错误

#define USER_ONLINE 0
#define USER_OFFLINE 1

#define MAX_MSG (8*1024)

#define CHAT_RES_SUCC 0
#define CHAT_RES_FALT 1

#define ADD_FRI_RESULT_ACCP 0
#define ADD_FRI_RESULT_REFU 1
#define ADD_FRI_RESULT_OFF 2
#define ADD_FRI_RESULT_NOEXIT 3

#define duan_TCP 4321

using protType=unsigned int;

struct PROT_REGISTER_RQ{
    protType prottype;
    char tel[15];
    char nick[30];
    char passwd[20];

    PROT_REGISTER_RQ() : prottype(DEF_PROT_REGISTER_RQ), tel{0}, nick{0}, passwd{0} {}
};

struct PROT_REGISTER_RS{
    protType prottype;
    int result;
    PROT_REGISTER_RS() : prottype(DEF_PROT_REGISTER_RS), result(REGIS_SUCC) {}
};

struct PROT_LOGIN_RQ{
    protType prottype;
    char tel[15];
    char passwd[20];
    PROT_LOGIN_RQ() : prottype(DEF_PROT_LOGIN_RQ), tel{0}, passwd{0} {}
};

struct PROT_LOGIN_RS{
    protType prottype;
    int userid; //当前登录用户的id
    int result; //成功 失败 不存在

    PROT_LOGIN_RS() : prottype(DEF_PROT_LOGIN_RS), userid(0), result(0) {}
};

struct PROT_FRIEND_INFO{
    protType prottype;
    int userid;
    int imgid;
    int status;
    char nick[30];
    char feeling[100];
    PROT_FRIEND_INFO() : prottype(DEF_PROT_FRIEND_INFO), userid(0),
        imgid(0), status(USER_ONLINE), nick{0}, feeling{0} {}
};

struct PROT_CHAT_INFO_RQ{
    protType prottype;
    int userid;
    int friid;
    char msg[MAX_MSG];
    PROT_CHAT_INFO_RQ() : prottype(DEF_PROT_CHAT_INFO_RQ), userid(0), friid(0), msg{0} {}
};

struct PROT_CHAT_INFO_RS{
    protType prottype;
    int userid;
    int friid;
    int result;
    PROT_CHAT_INFO_RS() : prottype(DEF_PROT_CHAT_INFO_RS), userid(0), friid(0), result(CHAT_RES_SUCC) {}
};

struct PROT_ADD_FRIEND_RQ{
    protType prottype;
    int userid;
    char usernick[30];
    char frinick[30];
    PROT_ADD_FRIEND_RQ() : prottype(DEF_PROT_ADD_FRIEND_RQ), userid(0), usernick{0}, frinick{0} {}
};

struct PROT_ADD_FRIEND_RS{
    protType prottype;
    int result;
    int friid;
    char friNick[30];
    char userNick[30];
    int userid;
    PROT_ADD_FRIEND_RS() : prottype(DEF_PROT_ADD_FRIEND_RS), result(ADD_FRI_RESULT_ACCP),
        friid(0), friNick{0}, userNick{0}, userid(0) {}
};

struct PROT_FRIEND_OFFLINE{
    protType prottype;
    int userid;
    PROT_FRIEND_OFFLINE() : prottype(DEF_PROT_FRIEND_OFFLINE), userid(0) {}
};

#endif
