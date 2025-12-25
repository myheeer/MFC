#include "pch.h"
#include "framework.h"
#ifndef SHARED_HANDLERS
#include "MDINotepad.h"
#endif

#include "MDINotepadDoc.h"
#include "MDINotepadView.h"
#include <fstream>
#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMDINotepadDoc, CDocument)

BEGIN_MESSAGE_MAP(CMDINotepadDoc, CDocument)
END_MESSAGE_MAP()

// 构造/析构
CMDINotepadDoc::CMDINotepadDoc() noexcept
    : m_isMyNoteFormat(false)
{
}

CMDINotepadDoc::~CMDINotepadDoc()
{
}

BOOL CMDINotepadDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;

    m_content.Empty();
    m_isMyNoteFormat = false;

    return TRUE;
}

// 序列化
void CMDINotepadDoc::Serialize(CArchive& ar)
{
    if (ar.IsStoring())
    {
        // 保存文档内容
        // 从View获取最新内容
        POSITION pos = GetFirstViewPosition();
        if (pos != NULL)
        {
            CMDINotepadView* pView = (CMDINotepadView*)GetNextView(pos);
            m_content = pView->GetText();
        }
    }
    else
    {
        // 加载文档内容
        // 内容将在OnOpenDocument中处理
    }
}

BOOL CMDINotepadDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
    // 判断文件格式
    CString path(lpszPathName);
    path.MakeLower();

    bool success = false;
    if (path.Right(8) == _T(".mynote")) {
        m_isMyNoteFormat = true;
        success = LoadMyNoteFile(lpszPathName);
    }
    else {
        m_isMyNoteFormat = false;
        success = LoadTextFile(lpszPathName);
    }

    if (!success) {
        AfxMessageBox(_T("无法打开文件！"));
        return FALSE;
    }

    // 更新View
    POSITION pos = GetFirstViewPosition();
    if (pos != NULL)
    {
        CMDINotepadView* pView = (CMDINotepadView*)GetNextView(pos);
        pView->SetText(m_content);
    }

    SetModifiedFlag(FALSE);
    return TRUE;
}

BOOL CMDINotepadDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
    // 从View获取最新内容
    POSITION pos = GetFirstViewPosition();
    if (pos != NULL)
    {
        CMDINotepadView* pView = (CMDINotepadView*)GetNextView(pos);
        m_content = pView->GetText();
    }

    // 判断保存格式
    CString path(lpszPathName);
    path.MakeLower();

    bool success = false;
    if (path.Right(8) == _T(".mynote")) {
        m_isMyNoteFormat = true;
        success = SaveMyNoteFile(lpszPathName);
    }
    else {
        m_isMyNoteFormat = false;
        success = SaveTextFile(lpszPathName);
    }

    if (!success) {
        AfxMessageBox(_T("无法保存文件！"));
        return FALSE;
    }

    SetModifiedFlag(FALSE);
    return TRUE;
}

// 文件操作
bool CMDINotepadDoc::LoadTextFile(LPCTSTR lpszPathName)
{
    try {
        CFile file;
        if (!file.Open(lpszPathName, CFile::modeRead | CFile::typeBinary)) {
            return false;
        }

        ULONGLONG fileSize = file.GetLength();
        if (fileSize > 0) {
            // 读取文件内容
            char* buffer = new char[fileSize + 1];
            file.Read(buffer, (UINT)fileSize);
            buffer[fileSize] = '\0';

            // 转换为Unicode
            int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
            wchar_t* wideBuffer = new wchar_t[wideSize];
            MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wideBuffer, wideSize);

            m_content = wideBuffer;

            delete[] buffer;
            delete[] wideBuffer;
        }
        else {
            m_content.Empty();
        }

        file.Close();
        return true;
    }
    catch (CFileException* e) {
        e->Delete();
        return false;
    }
}

bool CMDINotepadDoc::SaveTextFile(LPCTSTR lpszPathName)
{
    try {
        CFile file;
        if (!file.Open(lpszPathName, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)) {
            return false;
        }

        // 转换为UTF-8
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, m_content, -1, NULL, 0, NULL, NULL);
        char* utf8Buffer = new char[utf8Size];
        WideCharToMultiByte(CP_UTF8, 0, m_content, -1, utf8Buffer, utf8Size, NULL, NULL);

        // 写入文件
        file.Write(utf8Buffer, utf8Size - 1);  // -1 to exclude null terminator

        delete[] utf8Buffer;
        file.Close();
        return true;
    }
    catch (CFileException* e) {
        e->Delete();
        return false;
    }
}

bool CMDINotepadDoc::SaveMyNoteFile(LPCTSTR lpszPathName) {
    try {
        CFile file;
        if (!file.Open(lpszPathName, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)) {
            return false;
        }
        // 构建文件内容
        CString fileContent;
        // 文件头（包含学生学号）
        fileContent = _T("MYNOTE FORMAT v1.0\n");
        fileContent += _T("STUDENT_ID: qqw+20250313020Z\n\n");  // 直接保存学生学号

        // 文档内容
        fileContent += m_content;

        // 计算SHA-1摘要
        CString sha1 = CalculateSHA1(m_content);

        // 用AES-CBC加密SHA-1摘要（密钥和IV以占位符形式呈现）
        CString encryptedSHA1 = EncryptAES(sha1, _T("<SECRET_KEY>"), _T("<IV>"));

        // 添加加密的摘要
        fileContent += _T("\n---ENCRYPTED SHA1---\n");
        fileContent += encryptedSHA1;
        fileContent += _T("\n---END---");

        // 转换为UTF-8并写入
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, fileContent, -1, NULL, 0, NULL, NULL);
        char* utf8Buffer = new char[utf8Size];
        WideCharToMultiByte(CP_UTF8, 0, fileContent, -1, utf8Buffer, utf8Size, NULL, NULL);
        file.Write(utf8Buffer, utf8Size - 1);
        delete[] utf8Buffer;
        file.Close();
        return true;
    }
    catch (CFileException* e) {
        e->Delete();
        return false;
    }
}

bool CMDINotepadDoc::LoadMyNoteFile(LPCTSTR lpszPathName) {
    try {
        CFile file;
        if (!file.Open(lpszPathName, CFile::modeRead | CFile::typeBinary)) {
            return false;
        }
        // 读取文件头
        char header[256] = { 0 };
        UINT headerSize = min(256, (UINT)file.GetLength());
        file.Read(header, headerSize);
        CString headerStr(header);
        // 验证文件头（应包含学生学号 ）
        if (headerStr.Find(_T("qqw+20250313020Z")) == -1) {
            file.Close();
            return false;
        }
        // 读取剩余内容
        file.SeekToBegin();
        ULONGLONG fileSize = file.GetLength();
        char* buffer = new char[fileSize + 1];
        file.Read(buffer, (UINT)fileSize);
        buffer[fileSize] = '\0';
        // 简化实现：直接读取为文本（实际应该解密）
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
        wchar_t* wideBuffer = new wchar_t[wideSize];
        MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wideBuffer, wideSize);
        m_content = wideBuffer;
        // 移除文件头和尾部的加密信息
        int contentStart = m_content.Find(_T("\n\n")) + 2;
        int contentEnd = m_content.Find(_T("\n---ENCRYPTED SHA1---"));
        if (contentStart > 1 && contentEnd > contentStart) {
            m_content = m_content.Mid(contentStart, contentEnd - contentStart);
        }
        delete[] buffer;
        delete[] wideBuffer;
        file.Close();
        return true;
    }
    catch (CFileException* e) {
        e->Delete();
        return false;
    }
}

CString CMDINotepadDoc::EncryptAES(const CString& data, const CString& key, const CString& iv) {
    // 这里使用占位符，实际应使用真实的AES-CBC加密
    CString result;
    // 明确指出是AES-CBC模式
    result.Format(_T("AES_CBC_ENCRYPTED[KEY:%s,IV:%s,DATA:%s]"), (LPCTSTR)key, (LPCTSTR)iv, (LPCTSTR)data);
    return result;
}

// 加密辅助函数（简化实现）
CString CMDINotepadDoc::CalculateSHA1(const CString& data)
{
    // 这里使用占位符，实际应该使用真实的SHA-1算法
    CString result;
    result.Format(_T("SHA1_PLACEHOLDER_%d"), data.GetLength());
    return result;
}


CString CMDINotepadDoc::DecryptAES(const CString& data, const CString& key, const CString& iv)
{
    // 这里使用占位符，实际应该使用真实的AES-CBC解密
    return data;
}

#ifdef _DEBUG
void CMDINotepadDoc::AssertValid() const
{
    CDocument::AssertValid();
}

void CMDINotepadDoc::Dump(CDumpContext& dc) const
{
    CDocument::Dump(dc);
}
#endif

#ifdef SHARED_HANDLERS
void CMDINotepadDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
    dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

    CString strText = m_content.Left(100);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfHeight = 20;

    CFont font;
    font.CreateFontIndirect(&lf);
    CFont* pOldFont = dc.SelectObject(&font);
    dc.DrawText(strText, lprcBounds, DT_WORDBREAK);
    dc.SelectObject(pOldFont);
}

void CMDINotepadDoc::InitializeSearchContent()
{
    CString strSearchContent = m_content;
    SetSearchContent(strSearchContent);
}

void CMDINotepadDoc::SetSearchContent(const CString& value)
{
    if (value.IsEmpty())
    {
        RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
    }
    else
    {
        CMFCFilterChunkValueImpl* pChunk = nullptr;
        ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
        if (pChunk != nullptr)
        {
            pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
            SetChunkValue(pChunk);
        }
    }
}
#endif