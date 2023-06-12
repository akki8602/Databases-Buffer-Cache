#include "page_cache_lru.hpp"
#include "utilities/exception.hpp"

LRUReplacementPageCache::LRUReplacementPage::LRUReplacementPage(
    int argPageSize, int argExtraSize, unsigned argPageId, bool argPinned)
    : Page(argPageSize, argExtraSize), pageId(argPageId), pinned(argPinned) {}


LRUReplacementPageCache::LRUReplacementPageCache(int pageSize, int extraSize)
    : PageCache(pageSize, extraSize) {}

void LRUReplacementPageCache::setMaxNumPages(int maxNumPages) {
  maxNumPages_ = maxNumPages;
  
  // Discard unpinned pages until the number of pages in the cache is less than
  // or equal to `maxNumPages_` or only pinned pages remain.
  for (auto pagesIterator = pages_.begin(); pagesIterator != pages_.end();) {
    if (getNumPages() <= maxNumPages_) {
      break;
    }
    // pagesIterator->second means value of pagesIterator, which is RandomReplacementPage
    // key = first, value = second
    if (!pagesIterator->second->pinned) {
      delete pagesIterator->second;
      pagesIterator = pages_.erase(pagesIterator);

      // if pagesIterator is in list then remove pagesIterator->first
      auto deleteID = std::find(recency_list.begin(), recency_list.end(), pagesIterator->first);
      if (deleteID != recency_list.end()) {
        recency_list.erase(deleteID);
      }

    } else {
      // no unpinned page -> go to next pagesIterator
      ++pagesIterator;
    }
  }
}

int LRUReplacementPageCache::getNumPages() const {
  return (int)pages_.size();
}

Page *LRUReplacementPageCache::fetchPage(unsigned pageId, bool allocate) {
  ++numFetches_;

  auto pagesIterator = pages_.find(pageId);
  if (pagesIterator != pages_.end()) {
    ++numHits_;
    pagesIterator->second->pinned = true;

    // remove from recency list if necessary
    auto it = std::find(recency_list.begin(), recency_list.end(), pagesIterator->second->pageId);
    if (it != recency_list.end()) {
      recency_list.erase(it);
    }
    return pagesIterator->second;
  }

  if (!allocate) {
    return nullptr;
  }

  if (getNumPages() < maxNumPages_) {
    auto page = new LRUReplacementPage(pageSize_, extraSize_, pageId, true);
    pages_.emplace(pageId, page);
    return page;
  }

  // find an unpinned pageId in recency list to replace

  /* 
  if (!recency_list.empty()) {
    auto pageIdToReplace = recency_list.front();
    recency_list.pop_front();

    for (auto pagesIterator = pages_.begin(); pagesIterator != pages_.end();) {
      if (pagesIterator->first == pageIdToReplace) {
        // check this is an unpinned page
        if (pagesIterator->second->pinned) {
          return nullptr;
        }

        LRUReplacementPage *page = pagesIterator->second;
        pages_.erase(pagesIterator);
        pages_.emplace(pageId, page);
      }
      ++pagesIterator;
    }
  }
  */

  LRUReplacementPage *unpinnedPage = nullptr;
  for (auto pageId : recency_list) {
    auto pagesIterator = pages_.find(pageId);
    if (pagesIterator != pages_.end() && !pagesIterator->second->pinned) {
      unpinnedPage = pagesIterator->second;
      break;
    }
  }

  if (unpinnedPage != nullptr) {
    pages_.erase(unpinnedPage->pageId);
    unpinnedPage->pageId = pageId;
    unpinnedPage->pinned = true;
    pages_.emplace(pageId, unpinnedPage);
    return unpinnedPage;
  }

  return nullptr;
}

void LRUReplacementPageCache::unpinPage(Page *page, bool discard) {
  auto *page_unpinned = (LRUReplacementPage *)page;

  if (discard || getNumPages() > maxNumPages_) {
    pages_.erase(page_unpinned->pageId);
    delete page_unpinned;
  } else {
    page_unpinned->pinned = false;

    // push the page to the recency list if necessary
    auto it = std::find(recency_list.begin(), recency_list.end(), page_unpinned->pageId);
    if (it == recency_list.end()) {
      recency_list.push_back(page_unpinned->pageId);
    }
  }
}

void LRUReplacementPageCache::changePageId(Page *page, unsigned newPageId) {
  auto *pagenew = (LRUReplacementPage *)page;

  // Remove the old page ID from `pages_` and change the page ID.
  int oldId = pagenew->pageId;
  pages_.erase(pagenew->pageId);
  pagenew->pageId = newPageId;

  // Attempt to insert a page with page ID `newPageId` into `pages_`.
  auto [pagesIterator, success] = pages_.emplace(newPageId, pagenew);

  // If a page with page ID `newPageId` is already in the cache, discard it.
  if (!success) {
    delete pagesIterator->second;
    pagesIterator->second = pagenew;
  }
  
  //Make the same change in the recency list: 
    //get the iterator for the oldId's position and udpate the pageId
    auto oldPage = std::find(recency_list.begin(), recency_list.end(), oldId);
      if (oldPage != recency_list.end()) {
        recency_list.erase(oldPage);
        // oldPage->pageId = newPageId;
      }

    //If a different page with newId already exists, replace it with a new page
    /*
    auto [recencyIterator, pass] = recency_list.emplace(newPageId, pagenew);
    if (!pass) {
       delete recencyIterator->second;
       recencyIterator->second = pagenew;
    }
    */
  
}

void LRUReplacementPageCache::discardPages(unsigned pageIdLimit) {
  for (auto pagesIterator = pages_.begin(); pagesIterator != pages_.end();) {
    if (pagesIterator->second->pageId >= pageIdLimit) {
      delete pagesIterator->second;
      pagesIterator = pages_.erase(pagesIterator);

      // if pagesIterator is in list then remove pagesIterator->first
      auto deleteID = std::find(recency_list.begin(), recency_list.end(), pagesIterator->first);
      if (deleteID != recency_list.end()) {
        recency_list.erase(deleteID);
      }

    } else {
      ++pagesIterator;
    }
  }
}