//
//  m3_code.c
//
//  Created by Steven Massey on 4/19/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

#include "m3_code.h"

// Code mapping page ops

M3CodeMappingPage *  NewCodeMappingPage   (u32 i_minCapacity);
void                 FreeCodeMappingPage  (M3CodeMappingPage * i_page);

//---------------------------------------------------------------------------------------------------------------------------------


IM3CodePage  NewCodePage  (u32 i_minNumLines)
{
    static u32 s_sequence = 0;

    IM3CodePage page;

    u32 pageSize = sizeof (M3CodePageHeader) + sizeof (code_t) * i_minNumLines;

    pageSize = (pageSize + (d_m3CodePageAlignSize-1)) & ~(d_m3CodePageAlignSize-1); // align
    m3Alloc ((void **) & page, u8, pageSize);

    if (page)
    {
        page->info.sequence = ++s_sequence;
        page->info.numLines = (pageSize - sizeof (M3CodePageHeader)) / sizeof (code_t);

        page->info.mapping = NewCodeMappingPage (page->info.numLines);

        if (!page->info.mapping)
        {
            m3Free (page);
            return NULL;
        }

        m3log (runtime, "new page: %p; seq: %d; bytes: %d; lines: %d", GetPagePC (page), page->info.sequence, pageSize, page->info.numLines);
    }

    return page;
}


void  FreeCodePages  (IM3CodePage * io_list)
{
    IM3CodePage page = * io_list;

    while (page)
    {
        m3log (code, "free page: %d; %p; util: %3.1f%%", page->info.sequence, page, 100. * page->info.lineIndex / page->info.numLines);

        IM3CodePage next = page->info.next;
        FreeCodeMappingPage (page->info.mapping);
        m3Free (page);
        page = next;
    }

    * io_list = NULL;
}


u32  NumFreeLines  (IM3CodePage i_page)
{
    d_m3Assert (i_page->info.lineIndex <= i_page->info.numLines);

    return i_page->info.numLines - i_page->info.lineIndex;
}


void  EmitWord_impl  (IM3CodePage i_page, void * i_word)
{                                                                       d_m3Assert (i_page->info.lineIndex+1 <= i_page->info.numLines);
    i_page->code [i_page->info.lineIndex++] = i_word;
}

void  EmitWord32  (IM3CodePage i_page, const u32 i_word)
{                                                                       d_m3Assert (i_page->info.lineIndex+1 <= i_page->info.numLines);
    * ((u32 *) & i_page->code [i_page->info.lineIndex++]) = i_word;
}

void  EmitWord64  (IM3CodePage i_page, const u64 i_word)
{
#if M3_SIZEOF_PTR == 4
                                                                        d_m3Assert (i_page->info.lineIndex+2 <= i_page->info.numLines);
    * ((u64 *) & i_page->code [i_page->info.lineIndex]) = i_word;
    i_page->info.lineIndex += 2;
#else
                                                                        d_m3Assert (i_page->info.lineIndex+1 <= i_page->info.numLines);
    * ((u64 *) & i_page->code [i_page->info.lineIndex]) = i_word;
    i_page->info.lineIndex += 1;
#endif
}


void  EmitMappingEntry  (IM3CodePage i_page, IM3Module i_module, u64 i_moduleOffset)
{
    M3CodeMappingPage * page = i_page->info.mapping;
    assert (page->size < page->capacity);

    M3CodeMapEntry * entry = & page->entries[page->size++];
    pc_t pc = GetPagePC (i_page);

    entry->pc = pc;
    entry->module = i_module;
    entry->moduleOffset = i_moduleOffset;
}


pc_t  GetPageStartPC  (IM3CodePage i_page)
{
    return & i_page->code [0];
}


pc_t  GetPagePC  (IM3CodePage i_page)
{
    if (i_page)
        return & i_page->code [i_page->info.lineIndex];
    else
        return NULL;
}


void  PushCodePage  (IM3CodePage * i_list, IM3CodePage i_codePage)
{
    IM3CodePage next = * i_list;
    i_codePage->info.next = next;
    * i_list = i_codePage;
}


IM3CodePage  PopCodePage  (IM3CodePage * i_list)
{
    IM3CodePage page = * i_list;
    * i_list = page->info.next;
    page->info.next = NULL;

    return page;
}



u32  FindCodePageEnd  (IM3CodePage i_list, IM3CodePage * o_end)
{
    u32 numPages = 0;
    * o_end = NULL;

    while (i_list)
    {
        * o_end = i_list;
        ++numPages;
        i_list = i_list->info.next;
    }

    return numPages;
}


u32  CountCodePages  (IM3CodePage i_list)
{
    IM3CodePage unused;
    return FindCodePageEnd (i_list, & unused);
}


IM3CodePage GetEndCodePage  (IM3CodePage i_list)
{
    IM3CodePage end;
    FindCodePageEnd (i_list, & end);

    return end;
}


bool  ContainsPC  (IM3CodePage i_page, pc_t i_pc)
{
    return GetPageStartPC (i_page) <= i_pc && i_pc < GetPagePC (i_page);
}


bool  MapPCToOffset  (IM3CodePage i_page, pc_t i_pc, IM3Module * o_module, u64 * o_moduleOffset)
{
    M3CodeMappingPage * mapping = i_page->info.mapping;

    u32 left = 0;
    u32 right = mapping->size;

    while (left < right)
    {
        u32 mid = left + (right - left) / 2;

        if (mapping->entries[mid].pc < i_pc)
        {
            left = mid + 1;
        }
        else if (mapping->entries[mid].pc > i_pc)
        {
            right = mid;
        }
        else
        {
            *o_module = mapping->entries[mid].module;
            *o_moduleOffset = mapping->entries[mid].moduleOffset;
            return true;
        }
    }

    // Getting here means left is now one more than the element we want.
    if (left > 0)
    {
        left--;
        *o_module = mapping->entries[left].module;
        *o_moduleOffset = mapping->entries[left].moduleOffset;
        return true;
    }
    else return false;
}


//---------------------------------------------------------------------------------------------------------------------------------


M3CodeMappingPage *  NewCodeMappingPage  (u32 i_minCapacity)
{
    M3CodeMappingPage * page;
    u32 pageSize = sizeof (M3CodeMappingPage) + sizeof (M3CodeMapEntry) * i_minCapacity;

    m3Alloc ((void **) & page, u8, pageSize);

    if (page)
    {
        page->size = 0;
        page->capacity = i_minCapacity;
    }

    return page;
}


void  FreeCodeMappingPage  (M3CodeMappingPage * i_page)
{
    m3Free (i_page);
}
