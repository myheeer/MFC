#pragma once

#include <vector>
#include <string>
#include <stack>

// 在 #include 之后添加这些注释
// 注意：ID_EDIT_FIND 和 ID_EDIT_REPLACE 使用 MFC 标准定义
// 这些ID在 afxres.h 中已预定义

class CMDINotepadDoc;

// 编辑操作类型
enum class EditActionType {
    Insert,
    Delete,
    Replace
};

// 编辑操作记录
struct EditAction {
    EditActionType type;
    int startPos;
    int endPos;
    CString oldText;
    CString newText;
};

class CMDINotepadView : public CScrollView
{

public:
#ifdef _DEBUG
    // 关键：让测试类成为友元，才能 new 和访问 private 成员
    friend class MDINotepadViewTest;
    friend class MDINotepadDocTest;
#endif

protected:
    CMDINotepadView() noexcept;
    DECLARE_DYNCREATE(CMDINotepadView)

#ifdef _DEBUG
    // 让测试类成为友元
    friend class MDINotepadViewTest;
    friend class MDINotepadDocTest;
#endif





    // 属性
public:
    CMDINotepadDoc* GetDocument() const;

    // 操作
public:
    // 文本操作
    void SetText(const CString& text);
    CString GetText() const;
    void Clear();
    void DeleteAll();  // 添加删除所有内容的函数声明

    // 编辑操作
    void InsertText(const CString& text);
    void DeleteSelection();
    void Undo();
    void Redo();
    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }

    // 主题切换
    void SetDarkTheme(bool dark);
    bool IsDarkTheme() const { return m_isDarkTheme; }

    // 行号显示
    void ShowLineNumbers(bool show);
    bool IsShowLineNumbers() const { return m_showLineNumbers; }

    // 查找与替换
    bool FindNext(bool searchDown = true);
    void FindText(const CString& findText, bool matchCase, bool searchDown);
    void ReplaceCurrent(const CString& replaceText);
    void ReplaceAll(const CString& findText, const CString& replaceText, bool matchCase);
    void SelectAll();


public:
    // 文本编辑相关成员
    std::vector<CString> m_lines;           // 文本行数组
    int m_caretLine;                        // 光标所在行
    int m_caretColumn;                      // 光标所在列
    int m_topLine;                          // 顶部显示行
    int m_leftColumn;                       // 左侧显示列

    // 撤销/重做
    std::stack<EditAction> m_undoStack;
    std::stack<EditAction> m_redoStack;

    // 选择相关
    bool m_hasSelection;
    int m_selStartLine;
    int m_selStartColumn;
    int m_selEndLine;
    int m_selEndColumn;

    // 字体和颜色
    CFont m_font;
    int m_charWidth;
    int m_charHeight;
    COLORREF m_textColor;
    COLORREF m_backgroundColor;
    COLORREF m_lineNumberColor;
    COLORREF m_lineNumberBgColor;

    // 显示设置
    bool m_showLineNumbers;
    int m_lineNumberWidth;
    bool m_isDarkTheme;
    COLORREF m_selectionBgColor; // 新增

    // 查找相关成员
    CString m_lastFindText;          // 上次查找的文本
    bool    m_lastMatchCase;         // 上次是否区分大小写
    bool    m_lastSearchDown;        // true=向下查找，false=向上查找
    int     m_currentFindLine;       // 当前找到的行（用于继续查找）
    int     m_currentFindColumn;     // 当前找到的列

    #ifdef _DEBUG
        // 允许单元测试直接构造和访问私有成员
        friend class MDINotepadDocTest;
        friend class MDINotepadViewTest;
    #endif

public:
    // 辅助方法
    void InitializeView();
    void UpdateScrollSizes();
    void CalculateCharSize();
    CPoint GetCaretScreenPosition(int line, int column);
    int CalculateCaretXPosition(int line, int column);
    void MoveCaret(int line, int column);
    void UpdateCaretPosition();
    void DrawLineNumber(CDC* pDC, int line, int yPos);
    void DrawTextLine(CDC* pDC, int line, int yPos);
    int GetCharacterWidth(TCHAR ch);
    void InsertTextAt(int line, int col, const CString& text);
    void DeleteTextAt(int line, int col, const CString& text);

    // 撤销/重做辅助
    void AddUndoAction(const EditAction& action);
    void ClearRedoStack();
    void ApplyEditAction(const EditAction& action, bool isUndo);

    // 重写
public:
    virtual void OnDraw(CDC* pDC);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual ~CMDINotepadView();

protected:
    virtual void OnInitialUpdate();
    virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
    virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif


    // 消息处理函数
protected:
    // 键盘消息
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

    // 鼠标消息
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

    // 焦点消息
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);

    // 编辑菜单
    afx_msg void OnEditUndo();
    afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
    afx_msg void OnEditRedo();
    afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
    afx_msg void OnEditSelectAll();
    afx_msg void OnEditDeleteAll();  // 添加消息处理函数声明
    afx_msg void OnEditFind();
    afx_msg void OnEditReplace();
    afx_msg void OnUpdateEditFind(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditReplace(CCmdUI* pCmdUI);

    // 视图菜单
    afx_msg void OnViewDarkTheme();
    afx_msg void OnUpdateViewDarkTheme(CCmdUI* pCmdUI);
    afx_msg void OnViewLineNumbers();
    afx_msg void OnUpdateViewLineNumbers(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG
inline CMDINotepadDoc* CMDINotepadView::GetDocument() const
{
    return reinterpret_cast<CMDINotepadDoc*>(m_pDocument);
}
#endif