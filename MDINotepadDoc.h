#pragma once

class CMDINotepadDoc : public CDocument
{


    // 让测试类可以访问 protected 构造函数和所有成员
    friend class MDINotepadDocTest;
    friend class MDINotepadViewTest;

protected:
    CMDINotepadDoc() noexcept;

    DECLARE_DYNCREATE(CMDINotepadDoc)

    class MDINotepadDocTest;

   
public: 
    // 属性

public :
    CString m_content;  // 文档内容
    bool m_isMyNoteFormat;  // 是否为.mynote格式

    // 操作
public:

    // 重写
public:
    virtual BOOL OnNewDocument();
    virtual void Serialize(CArchive& ar);
    virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
    virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);

#ifdef SHARED_HANDLERS
    virtual void InitializeSearchContent();
    virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif



    // 实现
public:
    virtual ~CMDINotepadDoc();

#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

private:
    bool LoadTextFile(LPCTSTR lpszPathName);
    bool SaveTextFile(LPCTSTR lpszPathName);
    bool LoadMyNoteFile(LPCTSTR lpszPathName);
    bool SaveMyNoteFile(LPCTSTR lpszPathName);
    CString CalculateSHA1(const CString& data);
    CString EncryptAES(const CString& data, const CString& key, const CString& iv);
    CString DecryptAES(const CString& data, const CString& key, const CString& iv);



protected:

    // 生成的消息映射函数
protected:
    DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
    void SetSearchContent(const CString& value);
#endif
};