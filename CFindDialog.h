// CFindDialog.h 修正后
#pragma once
#include <afxcmn.h>
#include "resource.h"

class CMDINotepadView; // 前向声明

class CFindDialog : public CDialogEx
{
    DECLARE_DYNAMIC(CFindDialog)

public:
    // 移除全局变量，改为传入视图指针
    CFindDialog(bool findOnly = true, CMDINotepadView* pView = nullptr, CWnd* pParent = nullptr);
    virtual ~CFindDialog();

    CString m_findText;
    CString m_replaceText;
    bool    m_replaceAll;
    bool    m_replaceCurrent;

    enum { IDD = IDD_FIND_REPLACE };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnBtnFindNext();
    afx_msg void OnBtnReplace();
    afx_msg void OnBtnReplaceAll();
    afx_msg void OnCancel();

    DECLARE_MESSAGE_MAP()

private:
    bool m_findOnly;
    CMDINotepadView* m_pView; // 存储视图指针
};