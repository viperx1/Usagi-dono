#include "truncatedresponseinfo.h"

TruncatedResponseInfo::TruncatedResponseInfo()
    : m_isTruncated(false)
    , m_fieldsParsed(0)
    , m_fmaskReceived(0)
    , m_amaskReceived(0)
{
}

void TruncatedResponseInfo::beginTruncatedResponse(const QString& tag, const QString& command)
{
    m_isTruncated = true;
    m_tag = tag;
    m_command = command;
    m_fieldsParsed = 0;
    m_fmaskReceived = 0;
    m_amaskReceived = 0;
}

void TruncatedResponseInfo::updateProgress(int fieldsParsed, unsigned int fmask, unsigned int amask)
{
    m_fieldsParsed = fieldsParsed;
    m_fmaskReceived = fmask;
    m_amaskReceived = amask;
}

void TruncatedResponseInfo::reset()
{
    m_isTruncated = false;
    m_tag.clear();
    m_command.clear();
    m_fieldsParsed = 0;
    m_fmaskReceived = 0;
    m_amaskReceived = 0;
}

bool TruncatedResponseInfo::isValid() const
{
    // If truncated, must have tag and command
    if (m_isTruncated && (m_tag.isEmpty() || m_command.isEmpty())) {
        return false;
    }
    
    // If not truncated, fields should be clear
    if (!m_isTruncated && (!m_tag.isEmpty() || !m_command.isEmpty() || m_fieldsParsed != 0)) {
        return false;
    }
    
    return true;
}
