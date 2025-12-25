// CFindDialog.cpp 修正后
#include "pch.h"
#include "CFindDialog.h"
#include "MDINotepadView.h"

IMPLEMENT_DYNAMIC(CFindDialog, CDialogEx)

CFindDialog::CFindDialog(bool findOnly, CMDINotepadView* pView, CWnd* pParent)
    : CDialogEx(IDD_FIND_REPLACE, pParent)
    , m_findOnly(findOnly)
    , m_pView(pView)
    , m_findText(_T(""))
    , m_replaceText(_T(""))
    , m_replaceAll(false)
    , m_replaceCurrent(false)
{
}

CFindDialog::~CFindDialog()
{
}

void CFindDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_FIND, m_findText);
    DDX_Text(pDX, IDC_EDIT_REPLACE, m_replaceText);

    // 根据模式显示/隐藏替换相关控件
    if (m_findOnly)
    {
        GetDlgItem(IDC_STATIC_REPLACE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_EDIT_REPLACE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BTN_REPLACE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BTN_REPLACE_ALL)->ShowWindow(SW_HIDE);
    }
}

BOOL CFindDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置对话框标题
    SetWindowText(m_findOnly ? _T("查找") : _T("查找和替换"));

    return TRUE;
}

BEGIN_MESSAGE_MAP(CFindDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_FIND_NEXT, &CFindDialog::OnBtnFindNext)
    ON_BN_CLICKED(IDC_BTN_REPLACE, &CFindDialog::OnBtnReplace)
    ON_BN_CLICKED(IDC_BTN_REPLACE_ALL, &CFindDialog::OnBtnReplaceAll)
    ON_BN_CLICKED(IDOK, &CFindDialog::OnOK)  // 添加确定按钮处理（如果需要）
    ON_BN_CLICKED(IDCANCEL, &CFindDialog::OnCancel)  // 添加取消按钮处理
END_MESSAGE_MAP()

// 在 OnBtnFindNext 函数中，确保调用视图的查找函数
void CFindDialog::OnBtnFindNext()
{
    UpdateData(TRUE); // 从控件获取最新的查找文本
    if (m_findText.IsEmpty()) {
        AfxMessageBox(_T("请输入要查找的内容"));
        GetDlgItem(IDC_EDIT_FIND)->SetFocus();
        return;
    }
    if (m_pView) {
        // 调用View的查找方法，总是区分大小写，向下查找
        m_pView->FindText(m_findText, TRUE, TRUE);
    }
    else {
        AfxMessageBox(_T("无法找到编辑视图"));
    }
    // 注意：这里不调用 EndDialog，窗口保持打开
}


void CFindDialog::OnBtnReplace()
{
    UpdateData(TRUE);

    if (m_findText.IsEmpty())
    {
        AfxMessageBox(_T("请输入要查找的文本！"));
        GetDlgItem(IDC_EDIT_FIND)->SetFocus();
        return;
    }

    if (m_pView)
    {
        // 标记为替换当前
        m_replaceCurrent = true;
        EndDialog(IDOK);
    }
}

void CFindDialog::OnBtnReplaceAll()
{
    UpdateData(TRUE);

    if (m_findText.IsEmpty())
    {
        AfxMessageBox(_T("请输入要查找的文本！"));
        GetDlgItem(IDC_EDIT_FIND)->SetFocus();
        return;
    }

    if (m_pView)
    {
        // 标记为全部替换
        m_replaceAll = true;
        EndDialog(IDOK);
    }
}

// 修改 OnCancel 函数为覆盖基类版本
void CFindDialog::OnCancel()
{
    CDialogEx::OnCancel();
}