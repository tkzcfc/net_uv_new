#pragma once
#include "Common.h"

NS_NET_UV_BEGIN

#define FREE_BLOCK(b) do{ fc_free(b->data); fc_free(b); }while(0)

class Buffer
{
	struct block
	{
		uint32_t dataLen;
		char* data;
		block* pre;
		block* next;
	};

	uint32_t m_blockSize;
	uint32_t m_curDataLength;
	block* m_headBlock;
	block* m_tailBlock;
public:
	Buffer() = delete;
	Buffer(const Buffer&) = delete;
	Buffer(uint32_t blockSize)
	{
		assert(blockSize > 0);
		m_blockSize = blockSize;
		m_curDataLength = 0;
		m_headBlock = createBlock();
		m_tailBlock = m_headBlock;
	}

	virtual ~Buffer()
	{
		clear();
		FREE_BLOCK(m_headBlock);
	}

	inline uint32_t getDataLength()
	{
		return m_curDataLength;
	}

	inline uint32_t getBlockSize()
	{
		return m_blockSize;
	}

	inline uint32_t getHeadDataLen()
	{
		assert(m_headBlock != NULL);
		return m_headBlock->dataLen;
	}

	void add(char* pData, uint32_t dataLen)
	{
		char* curData = pData;
		while (dataLen > 0)
		{
			// create a new node
			if (m_tailBlock->dataLen >= m_blockSize)
			{
				block* b = createBlock();
				m_tailBlock->next = b;
				b->pre = m_tailBlock;
				m_tailBlock = b;
			}
			uint32_t sub = m_blockSize - m_tailBlock->dataLen;
			uint32_t copylen = (sub > dataLen) ? dataLen : sub;

			memcpy(m_tailBlock->data + m_tailBlock->dataLen, curData, copylen);

			m_tailBlock->dataLen += copylen;
			m_curDataLength += copylen;
			curData = curData + copylen;
			dataLen -= copylen;
		}
	}

	bool pop(char* pOutData, uint32_t len)
	{
		if (len <= 0 || m_curDataLength < len)
			return false;

		uint32_t curIndex = 0;
		do
		{
			if (len - curIndex >= m_headBlock->dataLen)
			{
				if (pOutData)
				{
					memcpy(pOutData + curIndex, m_headBlock->data, m_headBlock->dataLen);
				}
				curIndex += m_headBlock->dataLen;

				if (m_headBlock->next == NULL)
				{
					m_headBlock->dataLen = 0;
				}
				else
				{
					auto tmp = m_headBlock;
					m_headBlock = m_headBlock->next;
					FREE_BLOCK(tmp);
				}
				if (curIndex >= len)
					break;
			}
			else
			{
				assert(curIndex < len);
				uint32_t diff = len - curIndex;
				if (pOutData)
				{
					memcpy(pOutData + curIndex, m_headBlock->data, diff);
				}
				m_headBlock->dataLen = m_headBlock->dataLen - diff;

				char* buf = (char*)fc_malloc(m_blockSize);
				memcpy(buf, m_headBlock->data + diff, m_headBlock->dataLen);

				fc_free(m_headBlock->data);
				m_headBlock->data = buf;
				break;
			}
		} while (m_headBlock);

		m_curDataLength -= len;

		return true;
	}

	bool get(char* pOutData)
	{
		if (m_curDataLength <= 0)
			return false;

		uint32_t curIndex = 0;
		block* curBlock = m_headBlock;
		do
		{
			memcpy(pOutData + curIndex, curBlock->data, curBlock->dataLen);
			curIndex += curBlock->dataLen;
			curBlock = curBlock->next;
		} while (curBlock);

		return true;
	}

	void clear()
	{
		while (m_tailBlock != m_headBlock)
		{
			block* pre = m_tailBlock->pre;
			pre->next = NULL;
			FREE_BLOCK(m_tailBlock);
			m_tailBlock = pre;
		}
		m_headBlock->dataLen = 0;
		m_headBlock->next = NULL;
		m_curDataLength = 0;
	}

	inline char* getHeadBlockData()
	{
		return m_headBlock->data;
	}

protected:
	block* createBlock()
	{
		auto p = (block*)fc_malloc(sizeof(block));
		p->next = NULL;
		p->pre = NULL;
		p->dataLen = 0;
		p->data = (char*)fc_malloc(m_blockSize);
		return p;
	}
};


NS_NET_UV_END