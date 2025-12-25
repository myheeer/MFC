#include "pch.h"
#include "framework.h"
#ifndef SHARED_HANDLERS
#include "MDINotepad.h"
#include <locale>
#include <codecvt>
#endif

#include "CFindDialog.h"     // 查找/替换对话框
#include "MDINotepadDoc.h"
#include "MDINotepadView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMDINotepadView, CScrollView)


BOOL IsDoubleByteChar(TCHAR ch)
{
    // 简单判断：如果字符值大于0x7F，可能是双字节字符（包括中文）
    // 更准确的判断需要根据字符集
    return (ch > 0x7F);
}
int CMDINotepadView::GetCharacterWidth(TCHAR ch)
{
    // 如果是中文字符或全角字符，宽度为2个英文字符
    if (IsDoubleByteChar(ch) || ch == L'　') { // 全角空格
        return m_charWidth * 2;
    }
    // 其他字符（英文字母、数字、符号等）宽度为1个英文字符
    return m_charWidth;
}

BEGIN_MESSAGE_MAP(CMDINotepadView, CScrollView)
    ON_WM_CHAR()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_VSCROLL()
    ON_WM_HSCROLL()
    ON_COMMAND(ID_EDIT_UNDO, &CMDINotepadView::OnEditUndo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &CMDINotepadView::OnUpdateEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &CMDINotepadView::OnEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &CMDINotepadView::OnUpdateEditRedo)
    ON_COMMAND(ID_VIEW_DARKTHEME, &CMDINotepadView::OnViewDarkTheme)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DARKTHEME, &CMDINotepadView::OnUpdateViewDarkTheme)
    ON_COMMAND(ID_VIEW_LINENUMBERS, &CMDINotepadView::OnViewLineNumbers)
    ON_UPDATE_COMMAND_UI(ID_VIEW_LINENUMBERS, &CMDINotepadView::OnUpdateViewLineNumbers)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_FILE_PRINT, &CScrollView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_DIRECT, &CScrollView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CScrollView::OnFilePrintPreview)
    // 删除全部命令（资源ID 请在资源中定义，例如 ID_EDIT_DELETE_ALL）
    ON_COMMAND(ID_EDIT_DELETE_ALL, &CMDINotepadView::OnEditDeleteAll)
    ON_COMMAND(ID_EDIT_FIND, &CMDINotepadView::OnEditFind)
    ON_COMMAND(ID_EDIT_REPLACE, &CMDINotepadView::OnEditReplace)
    ON_UPDATE_COMMAND_UI(ID_EDIT_FIND, &CMDINotepadView::OnUpdateEditFind)
    ON_UPDATE_COMMAND_UI(ID_EDIT_REPLACE, &CMDINotepadView::OnUpdateEditReplace)
END_MESSAGE_MAP()

// 构造/析构
CMDINotepadView::CMDINotepadView() noexcept
    : m_caretLine(0)
    , m_caretColumn(0)
    , m_topLine(0)
    , m_leftColumn(0)
    , m_hasSelection(false)
    , m_selStartLine(0)
    , m_selStartColumn(0)

    , m_selEndLine(0)
    , m_selEndColumn(0)
    , m_charWidth(8)
    , m_charHeight(16)
    , m_showLineNumbers(true)
    , m_lineNumberWidth(50)
    , m_isDarkTheme(false)
{
    m_textColor = RGB(0, 0, 0);
    m_backgroundColor = RGB(255, 255, 255);
    m_lineNumberColor = RGB(128, 128, 128);
    m_lineNumberBgColor = RGB(240, 240, 240);
    m_selectionBgColor = RGB(0, 0, 255);

    // 初始化一个空行
    m_lines.push_back(_T(""));
}

CMDINotepadView::~CMDINotepadView()
{
}

BOOL CMDINotepadView::PreCreateWindow(CREATESTRUCT& cs)
{
    return CScrollView::PreCreateWindow(cs);
}

// 初始化
void CMDINotepadView::OnInitialUpdate()
{
    CScrollView::OnInitialUpdate();
    InitializeView();
}

void CMDINotepadView::InitializeView()
{
    // 创建字体
    LOGFONT lf = { 0 };
    lf.lfHeight = -16;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, _T("Consolas"));
    m_font.CreateFontIndirect(&lf);

    CalculateCharSize();
    UpdateScrollSizes();
}

void CMDINotepadView::CalculateCharSize()
{
    CClientDC dc(this);
    CFont* pOldFont = dc.SelectObject(&m_font);

    TEXTMETRIC tm;
    dc.GetTextMetrics(&tm);
    m_charHeight = tm.tmHeight;
    m_charWidth = tm.tmAveCharWidth;

    dc.SelectObject(pOldFont);
}

void CMDINotepadView::UpdateScrollSizes()
{
    if (m_lines.empty()) return;

    // 计算最大行宽
    int maxWidth = 0;
    for (const auto& line : m_lines) {
        int width = line.GetLength();
        if (width > maxWidth) maxWidth = width;
    }

    // 设置滚动大小
    CSize totalSize(
        (maxWidth + 1) * m_charWidth + m_lineNumberWidth,
        m_lines.size() * m_charHeight
    );
    CSize pageSize(m_charWidth * 40, m_charHeight * 10);
    CSize lineSize(m_charWidth, m_charHeight);

    SetScrollSizes(MM_TEXT, totalSize, pageSize, lineSize);
}


// 查找下一个匹配项，返回是否找到
bool CMDINotepadView::FindNext(bool searchDown)
{
    if (m_lastFindText.IsEmpty())
        return false;

    // 确定起始搜索位置
    int startLine, startCol;
    if (m_hasSelection && searchDown) {
        // 从选区之后开始
        startLine = m_selEndLine;
        startCol = m_selEndColumn;
    }
    else {
        // 从光标之后开始
        startLine = m_caretLine;
        startCol = m_caretColumn;
        // 关键修复：为了避免重复找到同一个位置，起始列要+1
        // 但要注意不能超出当前行
        if (startCol < m_lines[startLine].GetLength()) {
            startCol++;
        }
        else {
            // 如果已在行尾，则移到下一行开头
            startLine++;
            startCol = 0;
        }
    }

    // 重置选择状态
    m_hasSelection = false;

    int totalLines = (int)m_lines.size();
    if (totalLines == 0) {
        AfxMessageBox(_T("找不到更多匹配项。"));
        return false;
    }

    // 决定搜索范围
    int beginLine, endLine;
    if (searchDown) {
        beginLine = startLine;
        endLine = totalLines;
    }
    else {
        // 向上查找（本需求可暂不实现，但为了健壮性保留框架）
        beginLine = startLine;
        endLine = -1;
    }

    // 主查找循环
    for (int line = beginLine; line < endLine; ++line) {
        // 处理行号回绕（循环查找）
        int currentLine = line % totalLines;

        const CString& text = m_lines[currentLine];
        int col;

        // 执行查找（此处简化，去掉大小写选项，按需求总是区分大小写）
        // 第一行需要考虑起始列，后续行从0开始
        int searchStart = (currentLine == startLine % totalLines && line == beginLine) ? startCol : 0;

        col = text.Find(m_lastFindText, searchStart);
        if (col != -1) {
            // 找到了！设置选择区域
            MoveCaret(currentLine, col);
            m_hasSelection = true;
            m_selStartLine = currentLine;
            m_selStartColumn = col;
            m_selEndLine = currentLine;
            m_selEndColumn = col + m_lastFindText.GetLength();
            if (m_selEndColumn > m_lines[currentLine].GetLength())
                m_selEndColumn = m_lines[currentLine].GetLength();

            UpdateCaretPosition();
            Invalidate();
            CString msg;
            msg.Format(_T("找到匹配项（第 %d 行，第 %d 列）。"), currentLine + 1, col + 1);
            AfxMessageBox(msg);
            return true;
        }

        // 如果我们已经回绕了一圈（即 line >= totalLines），说明没找到
        if (line >= totalLines) {
            break;
        }
    }

    // 如果向下查找未果，尝试从文档开头循环查找（仅当起始位置不是0时）
    if (searchDown && startLine > 0) {
        for (int line = 0; line < startLine; ++line) {
            const CString& text = m_lines[line];
            int col = text.Find(m_lastFindText, 0);
            if (col != -1) {
                // 找到了！
                MoveCaret(line, col);
                m_hasSelection = true;
                m_selStartLine = line;
                m_selStartColumn = col;
                m_selEndLine = line;
                m_selEndColumn = col + m_lastFindText.GetLength();
                if (m_selEndColumn > m_lines[line].GetLength())
                    m_selEndColumn = m_lines[line].GetLength();

                UpdateCaretPosition();
                Invalidate();
                CString msg;
                msg.Format(_T("找到匹配项（第 %d 行，第 %d 列）。"), line + 1, col + 1);
                AfxMessageBox(msg);
                return true;
            }
        }
    }

    AfxMessageBox(_T("找不到更多匹配项。"));
    return false;
}

// 公开的查找接口（供对话框调用）
void CMDINotepadView::FindText(const CString& findText, bool matchCase, bool searchDown)
{
    m_lastFindText = findText;  // 保持原始文本
    m_lastMatchCase = matchCase;
    m_lastSearchDown = searchDown;

    // 查找时从当前位置开始（包括当前位置）
    FindNext(searchDown);
}

void CMDINotepadView::ReplaceCurrent(const CString& replaceText)
{
    if (!m_hasSelection)
        return;

    // 直接替换当前选中的文本
    CString selectedText;
    if (m_selStartLine == m_selEndLine)
    {
        selectedText = m_lines[m_selStartLine].Mid(
            m_selStartColumn,
            m_selEndColumn - m_selStartColumn
        );
    }

    // 记录撤销操作
    EditAction action;
    action.type = EditActionType::Replace;
    action.startPos = m_selStartLine;
    action.endPos = m_selStartColumn;
    action.oldText = selectedText;
    action.newText = replaceText;
    AddUndoAction(action);

    // 执行替换
    if (m_selStartLine == m_selEndLine)
    {
        CString& line = m_lines[m_selStartLine];
        line.Delete(m_selStartColumn, m_selEndColumn - m_selStartColumn);
        line.Insert(m_selStartColumn, replaceText);

        // 移动光标到替换后的位置
        m_caretLine = m_selStartLine;
        m_caretColumn = m_selStartColumn + replaceText.GetLength();
        m_hasSelection = false;
    }

    UpdateScrollSizes();
    Invalidate();
    UpdateCaretPosition();
    GetDocument()->SetModifiedFlag();
}

void CMDINotepadView::ReplaceAll(const CString& findText, const CString& replaceText, bool matchCase)
{
    if (findText.IsEmpty())
        return;

    // 记录整个文档的原始状态用于撤销
    EditAction action;
    action.type = EditActionType::Replace;
    action.startPos = 0;
    action.endPos = 0;
    action.oldText = GetText();  // 保存原始文本

    CString searchText = findText;
    CString searchLower = findText;
    if (!matchCase)
        searchLower.MakeLower();

    int totalReplacements = 0;

    // 遍历每一行进行替换
    for (int i = 0; i < (int)m_lines.size(); ++i)
    {
        CString line = m_lines[i];
        CString lineLower = line;
        if (!matchCase)
            lineLower.MakeLower();

        int pos = 0;
        while ((pos = lineLower.Find(searchLower, pos)) != -1)
        {
            // 执行替换
            line.Delete(pos, findText.GetLength());
            line.Insert(pos, replaceText);

            // 更新搜索字符串（处理大小写）
            lineLower = line;
            if (!matchCase)
                lineLower.MakeLower();

            // 移动位置到替换后的位置
            pos += replaceText.GetLength();
            totalReplacements++;
        }

        m_lines[i] = line;
    }

    // 保存新文本用于撤销
    action.newText = GetText();
    if (totalReplacements > 0)
    {
        AddUndoAction(action);
    }

    // 显示替换结果信息
    if (totalReplacements > 0)
    {
        CString msg;
        msg.Format(_T("已完成 %d 处替换。"), totalReplacements);
        AfxMessageBox(msg);
    }
    else
    {
        AfxMessageBox(_T("未找到匹配的文本。"));
    }

    // 清除选择，更新显示
    m_hasSelection = false;
    UpdateScrollSizes();
    Invalidate();
    GetDocument()->SetModifiedFlag();
}

void CMDINotepadView::OnEditFind()
{
    CFindDialog dlg(TRUE, this);  // TRUE 表示仅查找模式，传入this指针
    if (dlg.DoModal() == IDOK)
    {
        // 这里不需要再次调用FindText，因为对话框已经调用了
        // 如果需要，可以在这里添加其他处理
    }
}
// 将光标移动到指定行和列（并调整滚动以确保光标可见）
void CMDINotepadView::MoveCaret(int line, int column)
{
    // 限制有效范围
    if (line < 0) line = 0;
    if (line >= (int)m_lines.size()) line = (int)m_lines.size() - 1;

    if (column < 0) column = 0;
    if (column > (int)m_lines[line].GetLength()) column = m_lines[line].GetLength();

    m_caretLine = line;
    m_caretColumn = column;

    // 确保光标可见：调整垂直滚动
    CRect clientRect;
    GetClientRect(&clientRect);
    int visibleLines = clientRect.Height() / m_charHeight;
    if (visibleLines <= 0) visibleLines = 1;

    if (m_caretLine < m_topLine)
    {
        m_topLine = m_caretLine;
    }
    else if (m_caretLine >= m_topLine + visibleLines)
    {
        m_topLine = m_caretLine - visibleLines + 1;
        if (m_topLine < 0) m_topLine = 0;
    }

    // 确保光标可见：调整水平滚动（考虑中英文宽度）
    int xPos = CalculateCaretXPosition(m_caretLine, m_caretColumn);
    int visibleWidth = clientRect.Width();
    if (m_showLineNumbers)
        visibleWidth -= m_lineNumberWidth;

    CSize scrollSize = GetTotalSize();
    if (xPos < m_leftColumn)
    {
        m_leftColumn = xPos;
    }
    else if (xPos >= m_leftColumn + visibleWidth)
    {
        m_leftColumn = xPos - visibleWidth + m_charWidth;  // 留一点余量
    }

    UpdateScrollSizes();
    UpdateCaretPosition();
    Invalidate();  // 重绘以显示新光标位置
}
void CMDINotepadView::OnEditReplace() {
    CFindDialog dlg(FALSE, this); // FALSE 表示查找+替换模式，传入this指针
    if (dlg.DoModal() == IDOK) {
        // 处理替换操作
        if (dlg.m_replaceAll) {
            // 添加全部替换功能
            ReplaceAll(dlg.m_findText, dlg.m_replaceText, TRUE);
        }
        else if (dlg.m_replaceCurrent) {
            ReplaceCurrent(dlg.m_replaceText); // 继续查找下一个
            FindNext(TRUE);
        }
        else {
            // 普通查找
        }
    }
}

// 绘制
void CMDINotepadView::OnDraw(CDC* pDC)
{
    CMDINotepadDoc* pDoc = GetDocument();
    ASSERT_VALID(pDoc);
    if (!pDoc) return;

    // 设置背景色
    CRect clientRect;
    GetClientRect(&clientRect);
    pDC->FillSolidRect(clientRect, m_backgroundColor);

    // 选择字体
    CFont* pOldFont = pDC->SelectObject(&m_font);
    pDC->SetTextColor(m_textColor);
    pDC->SetBkMode(TRANSPARENT);

    // 获取滚动位置
    CPoint scrollPos = GetScrollPosition();
    int firstLine = scrollPos.y / m_charHeight;
    int lastLine = min((int)m_lines.size(), firstLine + clientRect.Height() / m_charHeight + 1);

    // 绘制行号背景
    if (m_showLineNumbers) {
        CRect lineNumberRect(0, 0, m_lineNumberWidth, clientRect.Height());
        pDC->FillSolidRect(lineNumberRect, m_lineNumberBgColor);

        // 绘制分隔线
        CPen pen(PS_SOLID, 1, RGB(200, 200, 200));
        CPen* pOldPen = pDC->SelectObject(&pen);
        pDC->MoveTo(m_lineNumberWidth - 1, 0);
        pDC->LineTo(m_lineNumberWidth - 1, clientRect.Height());
        pDC->SelectObject(pOldPen);
    }

    // 绘制每一行
    for (int i = firstLine; i < lastLine; i++) {
        int yPos = i * m_charHeight - scrollPos.y;

        // 绘制行号
        if (m_showLineNumbers) {
            DrawLineNumber(pDC, i, yPos);
        }

        // 绘制文本
        DrawTextLine(pDC, i, yPos);
    }

    pDC->SelectObject(pOldFont);
}

void CMDINotepadView::DrawLineNumber(CDC* pDC, int line, int yPos)
{
    CString lineNumber;
    lineNumber.Format(_T("%4d"), line + 1);

    COLORREF oldColor = pDC->SetTextColor(m_lineNumberColor);
    CRect rect(0, yPos, m_lineNumberWidth - 5, yPos + m_charHeight);
    pDC->DrawText(lineNumber, &rect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    pDC->SetTextColor(oldColor);
}



void CMDINotepadView::DrawTextLine(CDC* pDC, int line, int yPos)
{
    if (line >= (int)m_lines.size())
        return;

    int xPos = m_showLineNumbers ? m_lineNumberWidth : 0;
    CPoint scrollPos = GetScrollPosition();
    xPos -= scrollPos.x;

    const CString& text = m_lines[line];
    if (text.IsEmpty())
        return;

    // 计算当前行的选择范围（列）
    int selStartCol = -1, selEndCol = -1;
    if (m_hasSelection) {
        if (m_selStartLine == m_selEndLine) {
            // 单行选择
            if (line == m_selStartLine) {
                selStartCol = min(m_selStartColumn, m_selEndColumn);
                selEndCol = max(m_selStartColumn, m_selEndColumn);
            }
        }
        else {
            // 多行选择
            if (line == m_selStartLine) {
                selStartCol = m_selStartColumn;
                selEndCol = text.GetLength(); // 到行尾
            }
            else if (line == m_selEndLine) {
                selStartCol = 0; // 从行首开始
                selEndCol = m_selEndColumn;
            }
            else if (line > m_selStartLine && line < m_selEndLine) {
                // 完全被选中的行
                selStartCol = 0;
                selEndCol = text.GetLength();
            }
        }
    }

    // 绘制背景（如果整行被选中）
    if (selStartCol == 0 && selEndCol == text.GetLength()) {
        CRect fullLineRect(xPos, yPos, xPos + CalculateCaretXPosition(line, text.GetLength()), yPos + m_charHeight);
        pDC->FillSolidRect(fullLineRect, m_selectionBgColor);
    }

    // 逐字符绘制，并处理部分选中
    int currentX = xPos;
    for (int i = 0; i < text.GetLength(); i++) {
        TCHAR ch = text[i];
        int charWidth = GetCharacterWidth(ch);

        bool isCharSelected = (i >= selStartCol && i < selEndCol);
        COLORREF oldBkColor = pDC->SetBkMode(OPAQUE);

        if (isCharSelected) {
            // 绘制高亮背景
            CRect charRect(currentX, yPos, currentX + charWidth, yPos + m_charHeight);
            pDC->FillSolidRect(charRect, m_selectionBgColor);
            pDC->SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT)); // 白色文字
        }
        else {
            pDC->SetBkMode(TRANSPARENT);
            pDC->SetTextColor(m_textColor);
        }

        // 绘制字符
        CString charStr(ch);
        pDC->TextOut(currentX, yPos, charStr);

        currentX += charWidth;
    }

    // 恢复背景模式
    pDC->SetBkMode(TRANSPARENT);
}

// 文本操作
void CMDINotepadView::SetText(const CString& text)
{
    m_lines.clear();

    // 分割文本为行
    int start = 0;
    while (start < text.GetLength()) {
        int end = text.Find(_T('\n'), start);
        if (end == -1) {
            CString line = text.Mid(start);
            // 移除行尾的\r
            line.TrimRight(_T('\r'));
            m_lines.push_back(line);
            break;
        }
        else {
            CString line = text.Mid(start, end - start);
            line.TrimRight(_T('\r'));
            m_lines.push_back(line);
            start = end + 1;
        }
    }

    if (m_lines.empty()) {
        m_lines.push_back(_T(""));
    }

    m_caretLine = 0;
    m_caretColumn = 0;
    m_hasSelection = false;

    UpdateScrollSizes();
    Invalidate();
}

CString CMDINotepadView::GetText() const
{
    CString result;
    for (size_t i = 0; i < m_lines.size(); i++) {
        result += m_lines[i];
        if (i < m_lines.size() - 1) {
            result += _T("\r\n");
        }
    }
    return result;
}

void CMDINotepadView::Clear()
{
    m_lines.clear();
    m_lines.push_back(_T(""));
    m_caretLine = 0;
    m_caretColumn = 0;
    m_hasSelection = false;

    // 清空撤销/重做栈
    while (!m_undoStack.empty()) m_undoStack.pop();
    while (!m_redoStack.empty()) m_redoStack.pop();

    UpdateScrollSizes();
    Invalidate();
}

void CMDINotepadView::InsertText(const CString& text)
{
    if (text.IsEmpty()) return;

    // 记录操作用于撤销
    EditAction action;
    action.type = EditActionType::Insert;
    action.startPos = m_caretLine;
    action.newText = text;
    AddUndoAction(action);

    // 在光标位置插入文本
    if (m_caretLine >= (int)m_lines.size()) {
        m_caretLine = m_lines.size() - 1;
    }

    CString& currentLine = m_lines[m_caretLine];
    if (m_caretColumn > currentLine.GetLength()) {
        m_caretColumn = currentLine.GetLength();
    }

    // 处理多行插入
    if (text.Find(_T('\n')) != -1) {
        CString leftPart = currentLine.Left(m_caretColumn);
        CString rightPart = currentLine.Mid(m_caretColumn);

        std::vector<CString> newLines;
        int start = 0;
        while (start < text.GetLength()) {
            int end = text.Find(_T('\n'), start);
            if (end == -1) {
                CString line = text.Mid(start);
                line.TrimRight(_T('\r'));
                newLines.push_back(line);
                break;
            }
            else {
                CString line = text.Mid(start, end - start);
                line.TrimRight(_T('\r'));
                newLines.push_back(line);
                start = end + 1;
            }
        }

        if (!newLines.empty()) {
            m_lines[m_caretLine] = leftPart + newLines[0];
            for (size_t i = 1; i < newLines.size(); i++) {
                m_lines.insert(m_lines.begin() + m_caretLine + i, newLines[i]);
            }
            int lastLineIndex = m_caretLine + newLines.size() - 1;
            m_lines[lastLineIndex] += rightPart;

            m_caretLine = lastLineIndex;
            m_caretColumn = m_lines[lastLineIndex].GetLength() - rightPart.GetLength();
        }
    }
    else {
        currentLine.Insert(m_caretColumn, text);
        m_caretColumn += text.GetLength();
    }

    UpdateScrollSizes();
    Invalidate();
    GetDocument()->SetModifiedFlag();
}

// 继续在下一个artifact中实现剩余方法...
// 撤销/重做功能
void CMDINotepadView::AddUndoAction(const EditAction& action)
{
    m_undoStack.push(action);
    ClearRedoStack();
}

void CMDINotepadView::ClearRedoStack()
{
    while (!m_redoStack.empty()) {
        m_redoStack.pop();
    }
}

// 辅助：在指定位置插入文本（支持多行 \r\n）
void CMDINotepadView::InsertTextAt(int line, int col, const CString& text)
{
    if (line < 0) line = 0;
    if (line >= (int)m_lines.size()) line = (int)m_lines.size() - 1;

    CString& cur = m_lines[line];
    if (col > cur.GetLength()) col = cur.GetLength();

    if (text.Find(_T('\n')) != -1) {
        CString leftPart = cur.Left(col);
        CString rightPart = cur.Mid(col);

        std::vector<CString> newLines;
        int start = 0;
        while (start < text.GetLength()) {
            int end = text.Find(_T('\n'), start);
            if (end == -1) {
                CString ln = text.Mid(start);
                ln.TrimRight(_T('\r'));
                newLines.push_back(ln);
                break;
            }
            else {
                CString ln = text.Mid(start, end - start);
                ln.TrimRight(_T('\r'));
                newLines.push_back(ln);
                start = end + 1;
            }
        }

        if (!newLines.empty()) {
            m_lines[line] = leftPart + newLines[0];
            for (size_t i = 1; i < newLines.size(); ++i) {
                m_lines.insert(m_lines.begin() + line + i, newLines[i]);
            }
            int lastLine = line + (int)newLines.size() - 1;
            m_lines[lastLine] += rightPart;
        }
    }
    else {
        cur.Insert(col, text);
    }
}

// 辅助：从指定位置删除与 text 内容完全匹配的文本（支持多行 \r\n）
// 假设文本确实在文档中从 (line,col) 开始存在并连续匹配
void CMDINotepadView::DeleteTextAt(int line, int col, const CString& text)
{
    if (line < 0 || line >= (int)m_lines.size()) return;
    CString& cur = m_lines[line];
    if (col > cur.GetLength()) col = cur.GetLength();

    if (text.Find(_T('\n')) != -1) {
        // 把 text 按行拆分
        std::vector<CString> parts;
        int start = 0;
        while (start < text.GetLength()) {
            int end = text.Find(_T('\n'), start);
            if (end == -1) {
                CString ln = text.Mid(start);
                ln.TrimRight(_T('\r'));
                parts.push_back(ln);
                break;
            }
            else {
                CString ln = text.Mid(start, end - start);
                ln.TrimRight(_T('\r'));
                parts.push_back(ln);
                start = end + 1;
            }
        }

        if (parts.empty()) return;

        CString left = m_lines[line].Left(col);
        int endLine = line + (int)parts.size() - 1;
        if (endLine >= (int)m_lines.size()) endLine = (int)m_lines.size() - 1;

        CString right;
        if (endLine < (int)m_lines.size()) {
            int lastPartLen = parts.back().GetLength();
            if (lastPartLen <= m_lines[endLine].GetLength()) {
                right = m_lines[endLine].Mid(lastPartLen);
            }
            else {
                right = _T("");
            }
        }

        // 重建起始行
        m_lines[line] = left + right;

        // 删除中间被移除的行
        for (int i = 0; i < (int)parts.size() - 1; ++i) {
            if (line + 1 < (int)m_lines.size()) {
                m_lines.erase(m_lines.begin() + line + 1);
            }
        }
    }
    else {
        // 单行删除
        m_lines[line].Delete(col, text.GetLength());
    }
}

void CMDINotepadView::ApplyEditAction(const EditAction& action, bool isUndo)
{
    // isUndo == true: 将 action 反向应用（撤销）
    // isUndo == false: 重新应用 action（重做）
    if (action.type == EditActionType::Insert) {
        if (isUndo) {
            // 撤销插入：删除 action.newText
            DeleteTextAt(action.startPos, action.endPos, action.newText);
            m_caretLine = action.startPos;
            m_caretColumn = action.endPos;
        }
        else {
            // 重做插入：插入 action.newText
            InsertTextAt(action.startPos, action.endPos, action.newText);
            // 更新光标到插入后位置（粗略设置）
            // 计算插入后的位置：如果多行则移到最后一行末尾，否则列偏移
            if (action.newText.Find(_T('\n')) != -1) {
                int lines = 0;
                for (int i = 0; i < action.newText.GetLength(); ++i) if (action.newText[i] == '\n') ++lines;
                m_caretLine = action.startPos + lines;
                // 计算最后一行的长度 after insert
                m_caretColumn = m_lines[m_caretLine].GetLength();
            }
            else {
                m_caretLine = action.startPos;
                m_caretColumn = action.endPos + action.newText.GetLength();
            }
        }
    }
    else if (action.type == EditActionType::Delete) {
        if (isUndo) {
            // 撤销删除：在 startPos,startCol 插入 oldText
            InsertTextAt(action.startPos, action.endPos, action.oldText);
            // 光标移动到插入后的结束位置（粗略）
            if (action.oldText.Find(_T('\n')) != -1) {
                int lines = 0;
                for (int i = 0; i < action.oldText.GetLength(); ++i) if (action.oldText[i] == '\n') ++lines;
                m_caretLine = action.startPos + lines;
                m_caretColumn = m_lines[m_caretLine].GetLength();
            }
            else {
                m_caretLine = action.startPos;
                m_caretColumn = action.endPos + action.oldText.GetLength();
            }
        }
        else {
            // 重做删除：从 startPos,startCol 删除 oldText
            DeleteTextAt(action.startPos, action.endPos, action.oldText);
            m_caretLine = action.startPos;
            m_caretColumn = action.endPos;
        }
    }
    else if (action.type == EditActionType::Replace) {
        if (isUndo) {
            // 撤销替换：删除 newText，插入 oldText
            DeleteTextAt(action.startPos, action.endPos, action.newText);
            InsertTextAt(action.startPos, action.endPos, action.oldText);
            m_caretLine = action.startPos;
            m_caretColumn = action.endPos + action.oldText.GetLength();
        }
        else {
            // 重做替换：删除 oldText，插入 newText
            DeleteTextAt(action.startPos, action.endPos, action.oldText);
            InsertTextAt(action.startPos, action.endPos, action.newText);
            m_caretLine = action.startPos;
            m_caretColumn = action.endPos + action.newText.GetLength();
        }
    }

    // 保证文档至少有一行
    if (m_lines.empty()) m_lines.push_back(_T(""));

    UpdateScrollSizes();
    Invalidate();
    UpdateCaretPosition();
}

void CMDINotepadView::Undo()
{
    if (m_undoStack.empty()) return;

    EditAction action = m_undoStack.top();
    m_undoStack.pop();

    ApplyEditAction(action, true);

    m_redoStack.push(action);
}

void CMDINotepadView::Redo()
{
    if (m_redoStack.empty()) return;

    EditAction action = m_redoStack.top();
    m_redoStack.pop();

    ApplyEditAction(action, false);

    m_undoStack.push(action);
}

// 主题切换
void CMDINotepadView::SetDarkTheme(bool dark)
{
    m_isDarkTheme = dark;

    if (dark) {
        m_textColor = RGB(220, 220, 220);
        m_backgroundColor = RGB(30, 30, 30);
        m_lineNumberColor = RGB(150, 150, 150);
        m_lineNumberBgColor = RGB(40, 40, 40);
    }
    else {
        m_textColor = RGB(0, 0, 0);
        m_backgroundColor = RGB(255, 255, 255);
        m_lineNumberColor = RGB(128, 128, 128);
        m_lineNumberBgColor = RGB(240, 240, 240);
    }

    Invalidate();
}

// 行号显示
void CMDINotepadView::ShowLineNumbers(bool show)
{
    m_showLineNumbers = show;
    UpdateScrollSizes();
    Invalidate();
}

// 消息处理
void CMDINotepadView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar < 32 && nChar != VK_RETURN && nChar != VK_TAB) {
        CScrollView::OnChar(nChar, nRepCnt, nFlags);
        return;
    }

    // 如果有选区，先删除选区（并记录为一次删除操作）
    if (m_hasSelection) {
        DeleteSelection();  // 这会自动记录删除操作
    }

    CString insertText;
    if (nChar == VK_RETURN) {
        insertText = _T("\r\n");
    }
    else if (nChar == VK_TAB) {
        insertText = _T("\t");
    }
    else {
        insertText = (TCHAR)nChar;
    }

    // 记录插入操作
    EditAction action;
    action.type = EditActionType::Insert;
    action.startPos = m_caretLine;
    action.endPos = m_caretColumn;
    action.newText = insertText;
    action.oldText = _T("");  // 插入时旧文本为空
    AddUndoAction(action);

    // 执行插入
    CString& line = m_lines[m_caretLine];
    line.Insert(m_caretColumn, insertText);

    // 更新光标位置
    if (nChar == VK_RETURN) {
        // 回车换行
        CString currentLine = line.Mid(m_caretColumn);
        line = line.Left(m_caretColumn);
        m_lines.insert(m_lines.begin() + m_caretLine + 1, currentLine);
        m_caretLine++;
        m_caretColumn = 0;
    }
    else {
        m_caretColumn += insertText.GetLength();
    }

    ClearRedoStack();
    UpdateScrollSizes();
    Invalidate();
    UpdateCaretPosition();
}

void CMDINotepadView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_BACK) {
        if (m_hasSelection) {
            DeleteSelection();  // 已记录删除
        }
        else if (m_caretColumn > 0 || m_caretLine > 0) {
            // 记录删除操作（删除前一个字符）
            EditAction action;
            action.type = EditActionType::Delete;
            action.newText = _T("");

            if (m_caretColumn > 0) {
                // 删除当前行前一个字符
                action.startPos = m_caretLine;
                action.endPos = m_caretColumn - 1;
                action.oldText = m_lines[m_caretLine].Mid(m_caretColumn - 1, 1);

                m_lines[m_caretLine].Delete(m_caretColumn - 1);
                m_caretColumn--;
            }
            else if (m_caretLine > 0) {
                // 合并行（删除换行符）
                int prevLineLen = m_lines[m_caretLine - 1].GetLength();
                action.startPos = m_caretLine - 1;
                action.endPos = prevLineLen;
                action.oldText = _T("\r\n");

                m_lines[m_caretLine - 1] += m_lines[m_caretLine];
                m_lines.erase(m_lines.begin() + m_caretLine);
                m_caretLine--;
                m_caretColumn = prevLineLen;
            }

            AddUndoAction(action);
            ClearRedoStack();
            UpdateScrollSizes();
            Invalidate();
            UpdateCaretPosition();
        }
    }
    else if (nChar == VK_DELETE) {
        if (m_hasSelection) {
            DeleteSelection();
        }
        else if (m_caretColumn < m_lines[m_caretLine].GetLength() || m_caretLine < (int)m_lines.size() - 1) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.newText = _T("");

            if (m_caretColumn < m_lines[m_caretLine].GetLength()) {
                action.startPos = m_caretLine;
                action.endPos = m_caretColumn;
                action.oldText = m_lines[m_caretLine].Mid(m_caretColumn, 1);

                m_lines[m_caretLine].Delete(m_caretColumn);
            }
            else if (m_caretLine < (int)m_lines.size() - 1) {
                // 删除下一行的换行符（合并行）
                action.startPos = m_caretLine;
                action.endPos = m_lines[m_caretLine].GetLength();
                action.oldText = _T("\r\n");

                m_lines[m_caretLine] += m_lines[m_caretLine + 1];
                m_lines.erase(m_lines.begin() + m_caretLine + 1);
            }

            AddUndoAction(action);
            ClearRedoStack();
            UpdateScrollSizes();
            Invalidate();
        }
    }
    else {
        CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
    }
}

void CMDINotepadView::DeleteSelection()
{
    if (!m_hasSelection) return;

    // 归一化选择范围
    int startLine = min(m_selStartLine, m_selEndLine);
    int endLine = max(m_selStartLine, m_selEndLine);
    int startCol = m_selStartColumn;
    int endCol = m_selEndColumn;
    if (startLine == endLine) {
        startCol = min(m_selStartColumn, m_selEndColumn);
        endCol = max(m_selStartColumn, m_selEndColumn);
    }
    else if (m_selStartLine > m_selEndLine) {
        startCol = m_selEndColumn;
        endCol = m_selStartColumn;
    }

    // 记录删除的文本
    CString deletedText;
    if (startLine == endLine) {
        deletedText = m_lines[startLine].Mid(startCol, endCol - startCol);
        m_lines[startLine].Delete(startCol, endCol - startCol);
    }
    else {
        // 多行选择
        deletedText = m_lines[startLine].Mid(startCol) + _T("\r\n");
        for (int i = startLine + 1; i < endLine; i++) {
            deletedText += m_lines[i] + _T("\r\n");
        }
        deletedText += m_lines[endLine].Left(endCol);

        m_lines[startLine].Delete(startCol);
        m_lines[startLine] += m_lines[endLine].Mid(endCol);
        for (int i = startLine + 1; i <= endLine; i++) {
            m_lines.erase(m_lines.begin() + startLine + 1);
        }
    }

    // 记录撤销动作
    EditAction action;
    action.type = EditActionType::Delete;
    action.startPos = startLine;
    action.endPos = startCol;
    action.oldText = deletedText;
    action.newText = _T("");
    AddUndoAction(action);

    // 重置选择和光标
    m_caretLine = startLine;
    m_caretColumn = startCol;
    m_hasSelection = false;

    ClearRedoStack();
    UpdateScrollSizes();
    Invalidate();
    UpdateCaretPosition();
}

void CMDINotepadView::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();

    // 计算点击位置对应的行列
    CPoint scrollPos = GetScrollPosition();

    // 加上滚动偏移，转换为文档坐标
    point.x += scrollPos.x;
    point.y += scrollPos.y;

    int line = point.y / m_charHeight;

    if (line >= 0 && line < (int)m_lines.size())
    {
        int xOffset = m_showLineNumbers ? m_lineNumberWidth : 0;
        int clickX = point.x - xOffset;

        const CString& currentLine = m_lines[line];

        // 遍历行中的字符，找到点击位置对应的列
        int column = 0;
        int currentX = 0;

        for (int i = 0; i < currentLine.GetLength(); i++) {
            TCHAR ch = currentLine[i];
            int charWidth = GetCharacterWidth(ch);

            // 如果点击位置在字符的中间或右侧，则认为是下一个字符位置
            if (clickX >= currentX + charWidth / 2) {
                currentX += charWidth;
                column = i + 1;
            }
            else {
                // 点击位置在字符的左侧
                break;
            }
        }

        // 如果点击位置超过所有字符，则放在行尾
        if (clickX >= currentX) {
            column = currentLine.GetLength();
        }

        // 更新光标位置
        m_caretLine = line;
        m_caretColumn = column;

        // 如果按住 Shift 键，扩展选择（可选，后续可加）
        // 目前先简单实现点击定位
        if (!(nFlags & MK_SHIFT))
        {
            m_hasSelection = false;
        }
        else
        {
            // TODO: 实现 Shift + 点击扩展选择
        }

        UpdateCaretPosition();
        Invalidate();
    }

    CScrollView::OnLButtonDown(nFlags, point);
}


void CMDINotepadView::OnKillFocus(CWnd* pNewWnd)
{
    HideCaret();
    DestroyCaret();
    CScrollView::OnKillFocus(pNewWnd);
}

// 命令处理
void CMDINotepadView::OnEditUndo()
{
    Undo();
}

void CMDINotepadView::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CanUndo());
}

void CMDINotepadView::OnEditRedo()
{
    Redo();
}





void CMDINotepadView::OnUpdateEditRedo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CanRedo());
}

void CMDINotepadView::OnViewDarkTheme()
{
    SetDarkTheme(!m_isDarkTheme);
}



void CMDINotepadView::OnUpdateViewDarkTheme(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_isDarkTheme ? 1 : 0);
}

void CMDINotepadView::OnViewLineNumbers()
{
    ShowLineNumbers(!m_showLineNumbers);
}

void CMDINotepadView::OnUpdateViewLineNumbers(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_showLineNumbers ? 1 : 0);
}

// 打印支持
BOOL CMDINotepadView::OnPreparePrinting(CPrintInfo* pInfo)
{
    return DoPreparePrinting(pInfo);
}

void CMDINotepadView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
}

void CMDINotepadView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
}


// 计算光标在文本区域内的 X 坐标（相对于客户区左边，不含行号区域）
int CMDINotepadView::CalculateCaretXPosition(int line, int column)
{
    // 限制 column 不超出当前行长度
    if (line < 0 || line >= (int)m_lines.size())
        return 0;

    int lineLength = m_lines[line].GetLength();
    if (column > lineLength)
        column = lineLength;

    // 计算光标在行内的像素位置
    // 需要根据字符宽度累加计算，不能简单乘以列数
    int xPos = 0;

    // 遍历行中 column 之前的字符，累加每个字符的宽度
    for (int i = 0; i < column; i++) {
        TCHAR ch = m_lines[line][i];
        xPos += GetCharacterWidth(ch);
    }

    return xPos;
}

// 获取光标在客户区中的屏幕坐标（已考虑滚动和行号偏移）
CPoint CMDINotepadView::GetCaretScreenPosition(int line, int column)
{
    int xPos = CalculateCaretXPosition(line, column);

    // 如果显示行号，偏移行号区域宽度
    if (m_showLineNumbers)
    {
        xPos += m_lineNumberWidth;
    }

    int yPos = (line - m_topLine) * m_charHeight;  // 相对于可见区域顶部

    // 减去滚动偏移（GetScrollPosition() 返回已滚走的量）
    CPoint scrollPos = GetScrollPosition();
    xPos -= scrollPos.x;
    //yPos -= scrollPos.y;

    return CPoint(xPos, yPos);
}

void CMDINotepadView::OnUpdateEditFind(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);  // 始终启用查找功能
}

void CMDINotepadView::OnUpdateEditReplace(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);  // 始终启用替换功能
}

void CMDINotepadView::UpdateCaretPosition()
{
    if (GetFocus() != this)
        return;

    // 确保光标位置有效
    if (m_lines.empty())
    {
        m_caretLine = 0;
        m_caretColumn = 0;
    }
    else
    {
        if (m_caretLine >= (int)m_lines.size())
            m_caretLine = m_lines.size() - 1;
        if (m_caretColumn > m_lines[m_caretLine].GetLength())
            m_caretColumn = m_lines[m_caretLine].GetLength();
    }

    CPoint caretPos = GetCaretScreenPosition(m_caretLine, m_caretColumn);

    // 可选：限制光标不超出客户区（防止闪烁）
    CRect clientRect;
    GetClientRect(&clientRect);
    caretPos.x = max(0, caretPos.x);
    caretPos.y = max(0, min(caretPos.y, clientRect.Height() - m_charHeight));

    // 创建或调整光标（建议在 OnSetFocus 中创建一次）
    SetCaretPos(caretPos);
}
void CMDINotepadView::OnSetFocus(CWnd* pOldWnd)
{
    CScrollView::OnSetFocus(pOldWnd);
    CreateSolidCaret(2, m_charHeight);  // 宽度 2 像素是 Windows 编辑控件常见值
    UpdateCaretPosition();
    ShowCaret();
}

// 删除全部（记录撤销/重做）
void CMDINotepadView::DeleteAll()
{
    // 记录当前全部文本以便撤销
    EditAction action;
    action.type = EditActionType::Delete;
    action.startPos = 0;
    action.endPos = 0;
    action.oldText = GetText();
    action.newText = _T("");
    AddUndoAction(action);

    // 清空内容（保留一空行）
    m_lines.clear();
    m_lines.push_back(_T(""));
    m_caretLine = 0;
    m_caretColumn = 0;
    m_hasSelection = false;

    UpdateScrollSizes();
    Invalidate();
    GetDocument()->SetModifiedFlag();
    
    UpdateCaretPosition();
}

afx_msg void CMDINotepadView::OnEditDeleteAll()
{
    DeleteAll();
}

// 调试支持
#ifdef _DEBUG
void CMDINotepadView::AssertValid() const
{
    CScrollView::AssertValid();
}

void CMDINotepadView::Dump(CDumpContext& dc) const
{
    CScrollView::Dump(dc);
}

CMDINotepadDoc* CMDINotepadView::GetDocument() const
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMDINotepadDoc)));
    return (CMDINotepadDoc*)m_pDocument;
}
#endif