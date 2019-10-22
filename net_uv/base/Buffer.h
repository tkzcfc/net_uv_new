#pragma once
#include "Common.h"

NS_NET_UV_BEGIN

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
		fc_free(m_headBlock->data);
		fc_free(m_headBlock);
	}

	inline uint32_t getDataLength()
	{
		return m_curDataLength;
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
			fc_free(m_tailBlock->data);
			fc_free(m_tailBlock);
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