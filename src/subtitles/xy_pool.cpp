#include "stdafx.h"
#include "xy_pool.h"
#include <stdint.h>

XyPool::XyPool()
{
    memset(this, sizeof(*this), 0);
    m_blocks_full.next = &m_blocks_full;
    m_blocks_full.prev = &m_blocks_full;
    m_blocks_not_full.next = &m_blocks_not_full;
    m_blocks_not_full.prev = &m_blocks_not_full;
}

XyPool::~XyPool()
{
    BlockNode * iter1=m_blocks_not_full.next;
    while (iter1 != &m_blocks_not_full)
    {
        BlockNode * iter2=iter1->next;
        delete[] iter1->block_base;
        delete   iter1;
        iter1 = iter2;
    }
    iter1 = m_blocks_full.next;
    while (iter1 != &m_blocks_full)
    {
        BlockNode * iter2=iter1->next;
        delete[] iter1->block_base;
        delete   iter1;
        iter1 = iter2;
    }
}

int XyPool::Init( unsigned int obj_size, unsigned int align, unsigned int grow_by )
{
    if (m_align!=0)
    {
        DeInit();
    }
    if (align==0 || obj_size==0 || grow_by==0)
    {
        return -1;
    }
    m_obj_size    = obj_size;
    m_align       = align;
    m_grow_by     = grow_by;
    CalculeSize();
    return 0;
}

void XyPool::DeInit()
{
    BlockNode * iter1=m_blocks_not_full.next;
    while (iter1 != &m_blocks_not_full)
    {
        BlockNode * iter2=iter1->next;
        delete[] iter1->block_base;
        delete   iter1;
        iter1 = iter2;
    }
    iter1 = m_blocks_full.next;
    while (iter1 != &m_blocks_full)
    {
        BlockNode * iter2=iter1->next;
        delete[] iter1->block_base;
        delete   iter1;
        iter1 = iter2;
    }
    memset(this, sizeof(*this), 0);
    m_blocks_full.next = &m_blocks_full;
    m_blocks_full.prev = &m_blocks_full;
    m_blocks_not_full.next = &m_blocks_not_full;
    m_blocks_not_full.prev = &m_blocks_not_full;
}

void* XyPool::Alloc()
{
    void * ret = NULL;
    if (m_blocks_not_full.next==&m_blocks_not_full) {
        Grow();
    }
    if (m_blocks_not_full.next!=&m_blocks_not_full)
    {
        BlockNode* block_head = m_blocks_not_full.next;

        PPaddedObj* free_head = (PPaddedObj*)(block_head->free_head);
        block_head->free_head = *free_head;
        *free_head = (PPaddedObj)block_head;
        ret = (char*)free_head + m_obj_offset;

        m_free_block_number -= (block_head->used_number==0);
        block_head->used_number++;
        if (!block_head->free_head)
        {
            BlockNode * tmp = block_head;
            block_head->prev->next  = block_head->next;
            block_head->next->prev  = block_head->prev;

            block_head->next        = m_blocks_full.next;
            block_head->prev        = &m_blocks_full;
            block_head->next->prev  = block_head;
            block_head->prev->next  = block_head;
        }
        
    }
    return ret;
}

void XyPool::Free( void* obj )
{
    if (obj)
    {
        PPaddedObj obj_node = (PPaddedObj)((char*)obj-m_obj_offset);
        BlockNode *block_node = (BlockNode*)*obj_node;

        block_node->used_number--;
        if (!block_node->free_head || !block_node->used_number)
        {
            block_node->next->prev = block_node->prev;
            block_node->prev->next = block_node->next;
        }
        if (!block_node->used_number && m_free_block_number>0)
        {
            delete[] block_node->block_base;
            delete   block_node;
            m_total_block_number--;
        }
        else 
        {
            if (!block_node->used_number)
            {
                //empty. insert to tail
                block_node->next = &m_blocks_not_full;
                block_node->prev = m_blocks_not_full.prev;
                m_free_block_number++;
            }
            else
            {
                block_node->next = m_blocks_not_full.next;
                block_node->prev = &m_blocks_not_full;
            }
            block_node->next->prev = block_node;
            block_node->prev->next = block_node;

            *obj_node = block_node->free_head;
            block_node->free_head = obj_node;
        }
    }
}

void XyPool::Grow()
{
    BlockNode * new_block = DEBUG_NEW BlockNode;
    if (new_block)
    {
        new_block->block_base = DEBUG_NEW char[(m_grow_by+1)*m_padded_size];
        if (!new_block->block_base)
        {
            delete new_block;
            return;
        }
        int tmp = (intptr_t)new_block->block_base % m_padded_size;
        char * block = new_block->block_base + (m_padded_size + m_padded_shift - tmp) % m_padded_size;
        new_block->block = block;

        new_block->used_number = 0;
        new_block->free_head   = PPaddedObj(block);
        for (unsigned i=0;i<m_grow_by-1;i++, block += m_padded_size)
        {
            *PPaddedObj(block) = block + m_padded_size;
        }
        *PPaddedObj(block) = NULL;

        new_block->prev = &m_blocks_not_full;
        new_block->next =  m_blocks_not_full.next;
        new_block->prev->next = new_block;
        new_block->next->prev = new_block;

        m_free_block_number++;
        m_total_block_number++;
    }
}

void XyPool::Shrink()
{
    while (m_free_block_number>=2)
    {
        BlockNode * iter1=m_blocks_not_full.prev;
        ASSERT(iter1!=&m_blocks_not_full);
        iter1->next->prev = iter1->prev;
        iter1->prev->next = iter1->next;
        delete[] iter1->block_base;
        delete   iter1;
        m_free_block_number--;
        m_total_block_number--;
    }
}

void XyPool::CalculeSize()
{
    struct TestAlign
    {
        char c;
        PPaddedObj a;
    };
    const int p_padded_obj_alignment = (int)(&((TestAlign*)0)->a);

    if (m_align%p_padded_obj_alignment==0)
    {
        /*  |-obj-...-next-...|-obj-...-next-...|... */
        m_padded_shift = m_obj_size + (p_padded_obj_alignment - m_obj_size%p_padded_obj_alignment)%p_padded_obj_alignment;
        m_padded_size  = m_padded_shift + sizeof(PPaddedObj);
        m_padded_size += m_align - 1;
        m_padded_size -= m_padded_size % m_align;
        m_obj_offset   = m_padded_size - m_padded_shift;
    }
    else if (p_padded_obj_alignment%m_align==0)
    {
        /*  |-next-...-obj-...|-next-...-obj-...|... */
        m_obj_offset = sizeof(PPaddedObj) + (m_align - sizeof(PPaddedObj)%m_align)%m_align;
        m_padded_size  = m_obj_offset + m_obj_size;
        m_padded_size += p_padded_obj_alignment - 1;
        m_padded_size -= m_padded_size % p_padded_obj_alignment;
        m_padded_shift = 0;
    }
    else
    {
        ASSERT(0);
        m_padded_shift = m_obj_size;
        m_padded_size  = m_padded_shift + sizeof(PPaddedObj);
        m_padded_size += m_align - 1;
        m_padded_size -= m_padded_size % m_align;
        m_obj_offset   = m_padded_size - m_padded_shift;
    }
}
