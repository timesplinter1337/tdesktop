// ----------------------------------------------------
// Telegram Desktop Plugins Docs Logic
// ----------------------------------------------------

document.addEventListener('DOMContentLoaded', () => {
  // 1. Theme Switcher
  const htmlEl = document.documentElement;
  const themeBtn = document.getElementById('themeToggleBtn');
  const savedTheme = localStorage.getItem('tdesktop-docs-theme') || 'dark';

  htmlEl.setAttribute('data-theme', savedTheme);

  if (themeBtn) {
    themeBtn.addEventListener('click', () => {
      const current = htmlEl.getAttribute('data-theme') || 'dark';
      const next = current === 'dark' ? 'light' : 'dark';
      htmlEl.setAttribute('data-theme', next);
      localStorage.setItem('tdesktop-docs-theme', next);
    });
  }

  // 2. Mobile Drawer
  const mobileMenuBtn = document.getElementById('mobileMenuBtn');
  const sidebarLeft = document.getElementById('sidebarLeft');

  if (mobileMenuBtn && sidebarLeft) {
    mobileMenuBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      sidebarLeft.classList.toggle('open');
    });

    document.addEventListener('click', (e) => {
      if (!sidebarLeft.contains(e.target) && !mobileMenuBtn.contains(e.target)) {
        sidebarLeft.classList.remove('open');
      }
    });

    sidebarLeft.querySelectorAll('.nav-link').forEach(link => {
      link.addEventListener('click', () => {
        sidebarLeft.classList.remove('open');
      });
    });
  }

  // 3. Code Copy Buttons
  document.querySelectorAll('.copy-btn').forEach(button => {
    button.addEventListener('click', async () => {
      const targetId = button.getAttribute('data-target');
      let textToCopy = '';

      if (targetId) {
        const targetEl = document.getElementById(targetId);
        if (targetEl) textToCopy = targetEl.innerText;
      } else {
        const pre = button.closest('.code-box')?.querySelector('pre code');
        if (pre) textToCopy = pre.innerText;
      }

      if (!textToCopy) return;

      try {
        await navigator.clipboard.writeText(textToCopy);
        const originalText = button.innerText;
        button.innerText = 'Скопировано!';
        button.classList.add('copied');

        setTimeout(() => {
          button.innerText = originalText;
          button.classList.remove('copied');
        }, 2000);
      } catch (err) {
        console.error('Failed to copy: ', err);
      }
    });
  });

  // 4. ScrollSpy (Active Heading Highlighting)
  const sections = Array.from(document.querySelectorAll('.content-section'));
  const tocLinks = Array.from(document.querySelectorAll('.toc-link'));
  const navLinks = Array.from(document.querySelectorAll('.nav-link'));

  function updateActiveLink() {
    const scrollPos = window.scrollY + 120;
    let currentId = '';

    for (const section of sections) {
      const top = section.offsetTop;
      const height = section.offsetHeight;
      if (scrollPos >= top && scrollPos < top + height) {
        currentId = section.getAttribute('id');
        break;
      }
    }

    if (!currentId && sections.length > 0) {
      if (window.scrollY < sections[0].offsetTop) {
        currentId = sections[0].getAttribute('id');
      } else {
        currentId = sections[sections.length - 1].getAttribute('id');
      }
    }

    if (currentId) {
      tocLinks.forEach(link => {
        const href = link.getAttribute('href').replace('#', '');
        link.classList.toggle('active', href === currentId);
      });

      navLinks.forEach(link => {
        const href = link.getAttribute('href').replace('#', '');
        link.classList.toggle('active', href === currentId);
      });
    }
  }

  window.addEventListener('scroll', updateActiveLink, { passive: true });
  updateActiveLink();

  // 5. Client-Side Instant Search
  const searchTrigger = document.getElementById('searchTrigger');
  const searchModal = document.getElementById('searchModal');
  const searchInput = document.getElementById('searchInput');
  const searchResults = document.getElementById('searchResults');

  // Build searchable index from DOM
  const searchIndex = [];
  sections.forEach(sec => {
    const id = sec.getAttribute('id');
    const title = sec.querySelector('h2')?.innerText || id;

    // Sub-cards or general text
    const cards = sec.querySelectorAll('.api-card');
    if (cards.length > 0) {
      cards.forEach(card => {
        const sig = card.querySelector('.api-signature')?.innerText || '';
        const desc = card.querySelector('p')?.innerText || '';
        searchIndex.push({
          id,
          title: `${title} → ${sig}`,
          snippet: desc,
          keywords: (sig + ' ' + desc).toLowerCase()
        });
      });
    } else {
      const text = Array.from(sec.querySelectorAll('p, li')).map(el => el.innerText).join(' ');
      searchIndex.push({
        id,
        title,
        snippet: text.slice(0, 140) + '...',
        keywords: (title + ' ' + text).toLowerCase()
      });
    }
  });

  function openSearch() {
    if (!searchModal) return;
    searchModal.classList.add('open');
    searchInput.value = '';
    renderSearchResults('');
    setTimeout(() => searchInput.focus(), 50);
  }

  function closeSearch() {
    if (!searchModal) return;
    searchModal.classList.remove('open');
  }

  function renderSearchResults(query) {
    if (!searchResults) return;
    const cleanQuery = query.trim().toLowerCase();

    if (!cleanQuery) {
      searchResults.innerHTML = `
        <div class="no-results">Введите название функции, хука или команды...</div>
      `;
      return;
    }

    const matched = searchIndex.filter(item => item.keywords.includes(cleanQuery));

    if (matched.length === 0) {
      searchResults.innerHTML = `
        <div class="no-results">Ничего не найдено по запросу «${cleanQuery}»</div>
      `;
      return;
    }

    searchResults.innerHTML = matched.slice(0, 8).map(item => `
      <a href="#${item.id}" class="search-result-item">
        <div class="result-title">${escapeHtml(item.title)}</div>
        <div class="result-snippet">${escapeHtml(item.snippet)}</div>
      </a>
    `).join('');

    // Attach click handlers to close modal
    searchResults.querySelectorAll('.search-result-item').forEach(item => {
      item.addEventListener('click', () => {
        closeSearch();
      });
    });
  }

  function escapeHtml(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  if (searchTrigger) {
    searchTrigger.addEventListener('click', openSearch);
  }

  if (searchInput) {
    searchInput.addEventListener('input', (e) => {
      renderSearchResults(e.target.value);
    });
  }

  if (searchModal) {
    searchModal.addEventListener('click', (e) => {
      if (e.target === searchModal) closeSearch();
    });
  }

  // Keyboard shortcut '/' or 'Ctrl+K' / 'Cmd+K' and 'Escape'
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && searchModal?.classList.contains('open')) {
      closeSearch();
    } else if ((e.key === '/' || ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k')) && !searchModal?.classList.contains('open')) {
      // Don't trigger if user is already typing in an input
      if (['INPUT', 'TEXTAREA'].includes(document.activeElement?.tagName)) return;
      e.preventDefault();
      openSearch();
    }
  });
});
