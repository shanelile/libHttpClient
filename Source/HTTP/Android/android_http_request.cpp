#include "pch.h"

#include "android_http_request.h"

#include <httpClient/httpClient.h>

/* static */ JavaVM* HttpRequest::s_javaVM = nullptr;
/* static */ jclass HttpRequest::s_httpRequestClass = nullptr;
/* static */ jclass HttpRequest::s_httpResponseClass = nullptr;

/* static */ HRESULT HttpRequest::InitializeJavaEnvironment(JavaVM* javaVM) {
    s_javaVM = javaVM;
    JNIEnv* jniEnv = nullptr;

    // Java classes can only be resolved when we are on a Java-initiated thread; when we are on
    // a C++ background thread and attach to Java we do not have the full class-loader information.
    // This call should be made on JNI_OnLoad or another java thread and we will cache a global reference
    // to the classes we will use for making HTTP requests.
    jint result = s_javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6);

    if (result != JNI_OK) 
    {
        HC_TRACE_ERROR(HTTPCLIENT, "Failed to initialize because JavaVM is not attached to a java thread.");
        return E_HC_WRONG_THREAD;
    }

    jclass localHttpRequest = jniEnv->FindClass("com/xbox/httpclient/HttpClientRequest");

    if (localHttpRequest == nullptr) 
    {
        return E_FAIL;
    }

    jclass localHttpResponse = jniEnv->FindClass("com/xbox/httpclient/HttpClientResponse");

    if (localHttpResponse == nullptr) 
    {
        return E_FAIL;
    }

    s_httpRequestClass = reinterpret_cast<jclass>(jniEnv->NewGlobalRef(localHttpRequest));
    s_httpResponseClass = reinterpret_cast<jclass>(jniEnv->NewGlobalRef(localHttpResponse));
    return S_OK;
}

/* static */ void HttpRequest::CleanupJavaEnvironment() 
{
    JNIEnv* jniEnv = nullptr;
    bool isThreadAttached = false;
    jint getEnvResult = s_javaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_6);

    if (getEnvResult == JNI_EDETACHED) 
    {
        jint attachThreadResult = s_javaVM->AttachCurrentThread(&jniEnv, nullptr);

        if (attachThreadResult == JNI_OK) 
        {
            isThreadAttached = true;
        }
        else 
        {
            // TOOD: Throw?
        }
    }

    if (jniEnv != nullptr) 
    {
        jniEnv->DeleteGlobalRef(s_httpRequestClass);
        jniEnv->DeleteGlobalRef(s_httpResponseClass);
    }

    if (isThreadAttached) 
    {
        s_javaVM->DetachCurrentThread();
    }
}

HttpRequest::HttpRequest() : m_jniEnv(nullptr), m_httpRequestInstance(nullptr), m_httpResponseInstance(nullptr) {
    // TODO: move to an initialize thread so that we can return an error and push failures up the stack?
    s_javaVM->AttachCurrentThread(&m_jniEnv, nullptr);
    jmethodID httpRequestCtor = m_jniEnv->GetMethodID(s_httpRequestClass, "<init>", "()V");
    m_httpRequestInstance = m_jniEnv->NewObject(s_httpRequestClass, httpRequestCtor);
}

void HttpRequest::SetUrl(const char* url)
{
    jmethodID httpRequestSetUrlMethod = m_jniEnv->GetMethodID(s_httpRequestClass, "setHttpUrl", "(Ljava/lang/String;)V");
    jstring urlJstr = m_jniEnv->NewStringUTF(url);
    m_jniEnv->CallVoidMethod(m_httpRequestInstance, httpRequestSetUrlMethod, urlJstr);

    m_jniEnv->DeleteLocalRef(urlJstr);
}

void HttpRequest::AddHeader(const char* headerName, const char* headerValue)
{
    jmethodID httpRequestAddHeaderMethod = m_jniEnv->GetMethodID(s_httpRequestClass, "setHttpHeader", "(Ljava/lang/String;Ljava/lang/String;)V");
    jstring nameJstr = m_jniEnv->NewStringUTF(headerName);
    jstring valueJstr = m_jniEnv->NewStringUTF(headerValue);
    m_jniEnv->CallVoidMethod(m_httpRequestInstance, httpRequestAddHeaderMethod, nameJstr, valueJstr);

    m_jniEnv->DeleteLocalRef(nameJstr);
    m_jniEnv->DeleteLocalRef(valueJstr);
}

void HttpRequest::SetMethodAndBody(const char* method, const char* contentType, const uint8_t* body, uint32_t bodySize)
{
    jmethodID httpRequestSetBody = m_jniEnv->GetMethodID(s_httpRequestClass, "setHttpMethodAndBody", "(Ljava/lang/String;Ljava/lang/String;[B)V");
    jstring methodJstr = m_jniEnv->NewStringUTF(method);
    jstring contentTypeJstr = m_jniEnv->NewStringUTF(contentType);
    jbyteArray bodyArray = m_jniEnv->NewByteArray(bodySize);

    void *tempPrimitive = m_jniEnv->GetPrimitiveArrayCritical(bodyArray, 0);
    memcpy(tempPrimitive, body, bodySize);
    m_jniEnv->ReleasePrimitiveArrayCritical(bodyArray, tempPrimitive, 0);

    m_jniEnv->CallVoidMethod(m_httpRequestInstance, httpRequestSetBody, methodJstr, contentTypeJstr, bodyArray);

    m_jniEnv->DeleteLocalRef(methodJstr);
    m_jniEnv->DeleteLocalRef(contentTypeJstr);
}

void HttpRequest::ExecuteRequest() {
    // TODO: This can trigger a security exception if the app isn't configured for accessing the internet. Tweak the function so this can
    // be detected and bubbled back up.
    jmethodID httpRequestExecuteMethod = m_jniEnv->GetMethodID(s_httpRequestClass, "doRequest", "()Lcom/xbox/httpclient/HttpClientResponse;");
    m_httpResponseInstance = m_jniEnv->CallObjectMethod(m_httpRequestInstance, httpRequestExecuteMethod);
}

void HttpRequest::ProcessResponseBody(hc_call_handle_t call) 
{
    jmethodID httpResponseBodyMethod = m_jniEnv->GetMethodID(s_httpResponseClass, "getResponseBodyBytes", "()[B");
    jbyteArray responseBody = (jbyteArray)m_jniEnv->CallObjectMethod(m_httpResponseInstance, httpResponseBodyMethod);

    if (responseBody != nullptr) 
    {
        int bodySize = m_jniEnv->GetArrayLength(responseBody);
        if (bodySize > 0)
        {
            uint8_t* bodyBuffer = new uint8_t[bodySize];
            m_jniEnv->GetByteArrayRegion(responseBody, 0, bodySize, reinterpret_cast<jbyte*>(bodyBuffer));

            HCHttpCallResponseSetResponseBodyBytes(call, bodyBuffer, bodySize);
        }
    }
}

uint32_t HttpRequest::GetResponseCode() {
    // TODO: Verify that the response completed and didn't have an error
    jmethodID httpResponseStatusMethod = m_jniEnv->GetMethodID(s_httpResponseClass, "getResponseCode", "()I");
    jint responseStatus = m_jniEnv->CallIntMethod(m_httpResponseInstance, httpResponseStatusMethod);
    return (uint32_t)responseStatus;
}

uint32_t HttpRequest::GetResponseHeaderCount() {
    // TODO: Verify that the response completed and didn't have an error
    jmethodID httpResponssNumHeadersMethod = m_jniEnv->GetMethodID(s_httpResponseClass, "getNumHeaders", "()I");
    jint numHeaders = m_jniEnv->CallIntMethod(m_httpResponseInstance, httpResponssNumHeadersMethod);
    return (uint32_t)numHeaders;
}

std::string HttpRequest::GetHeaderNameAtIndex(uint32_t index) {
    if (m_httpResponseInstance != nullptr)
    {
        jmethodID httpRepsonseGetHeaderName = m_jniEnv->GetMethodID(s_httpResponseClass, "getHeaderNameAtIndex", "(I)Ljava/lang/String;");
        jstring headerName = (jstring)m_jniEnv->CallObjectMethod(m_httpResponseInstance, httpRepsonseGetHeaderName, index);
        const char* nameCstr = m_jniEnv->GetStringUTFChars(headerName, NULL);

        std::string headerStr(nameCstr);
        m_jniEnv->ReleaseStringUTFChars(headerName, nameCstr);

        return headerStr;
    }
    else
    {
        return nullptr;
    }
}

std::string HttpRequest::GetHeaderValueAtIndex(uint32_t index) {
    if (m_httpResponseInstance != nullptr)
    {
        jmethodID httpRepsonseGetHeaderValue = m_jniEnv->GetMethodID(s_httpResponseClass, "getHeaderValueAtIndex", "(I)Ljava/lang/String;");
        jstring headerValue = (jstring)m_jniEnv->CallObjectMethod(m_httpResponseInstance, httpRepsonseGetHeaderValue, index);
        const char* valueCstr = m_jniEnv->GetStringUTFChars(headerValue, NULL);

        std::string valueStr(valueCstr);
        m_jniEnv->ReleaseStringUTFChars(headerValue, valueCstr);

        return valueStr;
    }
    else
    {
        return nullptr;
    }
}

HttpRequest::~HttpRequest() {
    s_javaVM->DetachCurrentThread();
    m_jniEnv = nullptr;    
}
