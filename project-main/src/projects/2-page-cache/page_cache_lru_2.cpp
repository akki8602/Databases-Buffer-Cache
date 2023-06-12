#include "page_cache_lru_2.hpp"
#include "page_cache_lru.hpp"
#include "utilities/exception.hpp"



static auto compareLRU = [](const auto& value1, const auto& value2) {
        return value1.second[1] < value2.second[1];
};
static auto compareLRU2 = [](const auto& value1, const auto& value2) {
        return value1.second[0] < value2.second[0];
};



LRU2ReplacementPageCache::LRU2ReplacementPage::LRU2ReplacementPage(
    int argPageSize, int argExtraSize, unsigned argPageId, bool argPinned)
    : Page(argPageSize, argExtraSize), pageId(argPageId), pinned(argPinned) {}

LRU2ReplacementPageCache::LRU2ReplacementPageCache(int pageSize, int extraSize)
    : PageCache(pageSize, extraSize) {}

void LRU2ReplacementPageCache::setMaxNumPages(int maxNumPages) {
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

      // Modify to delete key-value pair from history
      // auto deleteID = std::find(history.begin(), history.end(), pagesIterator->first);
      auto deleteID = history.find(pagesIterator->first);
      if (deleteID != history.end()) {
        history.erase(deleteID);
      }

    } else {
      // no unpinned page -> go to next pagesIterator
      ++pagesIterator;
    }
  }
}

int LRU2ReplacementPageCache::getNumPages() const {
  //simply change pages_ to history
  return (int)history.size();
}

Page *LRU2ReplacementPageCache::fetchPage(unsigned pageId, bool allocate) {
  ++numFetches_;

  auto pagesIterator = pages_.find(pageId);
  if (pagesIterator != pages_.end()) {
    ++numHits_;
    pagesIterator->second->pinned = true;

    // No need to remove from history as we need to maintain timestamps.
    // Fetching(pinning) does not change the history in the case of a hit in cache.
    return pagesIterator->second;
  }

  if (!allocate) {
    return nullptr;
  }

  if (getNumPages() < maxNumPages_) {
    auto page = new LRU2ReplacementPage(pageSize_, extraSize_, pageId, true);
    pages_.emplace(pageId, page);
    //Add this page to history with entries 0,0 (?)
    history.insert(std::pair<unsigned, std::vector<unsigned>>(pageId, {0, 0}));
    return page;
  }

    std::vector<std::pair<unsigned, std::vector<unsigned>>> sorted_history(history.begin(), history.end());
    std::sort(sorted_history.begin(), sorted_history.end(), compareLRU2);

    LRU2ReplacementPage *unpinnedPage = nullptr;
    if (sorted_history.begin()->second[0] == 0) {
        int targetValue = 0;
        auto pagesWithSingleAccess = std::find_if(sorted_history.rbegin(), sorted_history.rend(), [targetValue](const std::pair<unsigned, std::vector<unsigned>>& p) {
            const std::vector<unsigned>& v = p.second;
            return v[0] == targetValue;
        });
        ++pagesWithSingleAccess;
        auto pagesWithSingleAccessIt = pagesWithSingleAccess.base();
        std::sort(sorted_history.begin(), pagesWithSingleAccessIt, compareLRU);
        for (const auto& entry : sorted_history) {
            auto pagesIterator = pages_.find(entry.first);
            if (pagesIterator != pages_.end() && !pagesIterator->second->pinned) {
                unpinnedPage = pagesIterator->second;
                break;
            }
        }
    } else {
        for (const auto& entry : sorted_history) {
            auto pagesIterator = pages_.find(entry.first);
            if (pagesIterator != pages_.end() && !pagesIterator->second->pinned) {
                unpinnedPage = pagesIterator->second;
                break;
            }
        }
    }

    if (unpinnedPage != nullptr) {
        unsigned idToErase = unpinnedPage->pageId;
        pages_.erase(unpinnedPage->pageId);
        history.erase(idToErase);
        unpinnedPage->pageId = pageId;
        unpinnedPage->pinned = true;
        pages_.emplace(pageId, unpinnedPage);
        history.insert(std::pair<unsigned, std::vector<unsigned>>(pageId, {0, 0}));
        return unpinnedPage;
    }

    return nullptr;
}

void LRU2ReplacementPageCache::unpinPage(Page *page, bool discard) {
  auto *page_unpinned = (LRU2ReplacementPage *)page;

  if (discard || getNumPages() > maxNumPages_) {
    unsigned idToErase = page_unpinned->pageId;
    pages_.erase(page_unpinned->pageId);
    delete page_unpinned;
    history.erase(idToErase);
    // Delete page from history
  } else {
    page_unpinned->pinned = false;
    // increment timestamp
    ++timeStamp;
    // Find the page in history, push LRU timestamp to LRU2 timestamp an update LRU timestamp to current timestamp
    //auto changeHistory = std::find(history.begin(), history.end(), page_unpinned->pageId);
    auto changeHistory = history.find(page_unpinned->pageId);
    if (changeHistory != history.end()) {
      changeHistory->second[0] = changeHistory->second[1];
      changeHistory->second[1] = timeStamp;
    }
  }
}

void LRU2ReplacementPageCache::changePageId(Page *page, unsigned newPageId) {
  auto *pagenew = (LRU2ReplacementPage *)page;

  // Remove the old page ID from `pages_` and change the page ID.
  int oldId = pagenew->pageId;

  pages_.erase(pagenew->pageId);
  pagenew->pageId = newPageId;

  // Attempt to insert a page with page ID `newPageId` into `pages_`.
  //Make the same change in history:
    //Mirror changes in the cache exactly and don't discard anything other than
    //the possible already existing page with same new ID

  // auto oldPage = std::find(history.begin(), history.end(), oldId);
  auto [pagesIterator, success] = pages_.emplace(newPageId, pagenew);

  /*
  // If a page with page ID `newPageId` is already in the cache, discard it.
  if (!success) {
    delete pagesIterator->second;
    pagesIterator->second = pagenew;
    if (oldPage != history.end()) {
      oldPage->second[0] = 0;
      oldPage->second[0] = 0;
    }
  }
  else {
    if (oldPage != history.end()) {
      oldPage->first = newPageId;
    }
  }
  */
  
  // If a page with page ID `newPageId` is already in the cache, discard it.
  if (!success) {
    delete pagesIterator->second;
    pagesIterator->second = pagenew;
  }

  // Make the same change in history:
  // Mirror changes in the cache exactly and don't discard anything other than
  // the possible already existing page with same new ID
  auto oldPage = history.find(oldId);
  if (oldPage != history.end()) {
    auto oldLRU = oldPage->second;
    history.erase(oldId);
    history.insert(std::pair<unsigned, std::vector<unsigned>>(newPageId, oldLRU));
  }
  
}

void LRU2ReplacementPageCache::discardPages(unsigned pageIdLimit) {
  for (auto pagesIterator = pages_.begin(); pagesIterator != pages_.end();) {
    if (pagesIterator->second->pageId >= pageIdLimit) {
      delete pagesIterator->second;
      pagesIterator = pages_.erase(pagesIterator);

      // Remove page from history
      // auto deleteID = std::find(history.begin(), history.end(), pagesIterator->first);
      auto deleteID = history.find(pagesIterator->first);
      if (deleteID != history.end()) {
        history.erase(deleteID);
      }

    } else {
      ++pagesIterator;
    }
  }
}
