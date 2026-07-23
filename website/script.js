const header = document.querySelector('.site-header');
const menuButton = document.querySelector('.menu-button');
const navLinks = document.querySelector('.nav-links');
const vibranceSlider = document.querySelector('#vibrance-slider');
const vibranceValue = document.querySelector('#vibrance-value');
const selectedGame = document.querySelector('#selected-game');
const selectedArt = document.querySelector('#selected-art');
const profileCards = document.querySelectorAll('.profile-card');
const saveProfile = document.querySelector('#save-profile');
const compareRange = document.querySelector('#compare-range');
const naturalScene = document.querySelector('#natural-scene');
const compareHandle = document.querySelector('#compare-handle');
const toast = document.querySelector('#toast');

const updateHeader = () => header.classList.toggle('scrolled', window.scrollY > 20);
window.addEventListener('scroll', updateHeader, { passive: true });
updateHeader();

menuButton?.addEventListener('click', () => {
  const open = !menuButton.classList.contains('open');
  menuButton.classList.toggle('open', open);
  navLinks.classList.toggle('open', open);
  menuButton.setAttribute('aria-expanded', String(open));
});

navLinks?.querySelectorAll('a').forEach(link => {
  link.addEventListener('click', () => {
    menuButton?.classList.remove('open');
    navLinks.classList.remove('open');
    menuButton?.setAttribute('aria-expanded', 'false');
  });
});

const setRangeProgress = (element, value) => {
  if (!element) return;
  element.style.setProperty('--range-progress', `${value}%`);
};

vibranceSlider?.addEventListener('input', event => {
  const value = event.target.value;
  if (vibranceValue) vibranceValue.textContent = `${value}%`;
  setRangeProgress(vibranceSlider, value);
  const activeCard = document.querySelector('.profile-card.active small');
  if (activeCard) activeCard.textContent = `Vibrance ${value}%`;
});
setRangeProgress(vibranceSlider, vibranceSlider?.value || 72);

profileCards.forEach(card => {
  card.addEventListener('click', () => {
    profileCards.forEach(item => item.classList.remove('active'));
    card.classList.add('active');

    const value = card.dataset.vibrance;
    selectedGame.textContent = card.dataset.game;
    vibranceSlider.value = value;
    vibranceValue.textContent = `${value}%`;
    setRangeProgress(vibranceSlider, value);

    const art = card.querySelector('.game-art');
    selectedArt.className = `selected-game-art ${[...art.classList].find(name => name.startsWith('art-'))}`;
    selectedArt.innerHTML = art.innerHTML;
  });
});

const showToast = (title, message) => {
  toast.querySelector('strong').textContent = title;
  toast.querySelector('small').textContent = message;
  toast.classList.add('show');
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => toast.classList.remove('show'), 3400);
};

saveProfile?.addEventListener('click', () => {
  const game = selectedGame.textContent;
  showToast('Profile saved', `${game} will use ${vibranceSlider.value}% vibrance.`);
});

compareRange?.addEventListener('input', event => {
  const value = event.target.value;
  naturalScene.style.width = `${value}%`;
  compareHandle.style.left = `${value}%`;
});

const revealObserver = new IntersectionObserver(entries => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      entry.target.classList.add('visible');
      revealObserver.unobserve(entry.target);
    }
  });
}, { threshold: 0.13 });

document.querySelectorAll('.reveal').forEach(element => revealObserver.observe(element));

document.querySelectorAll('.accordion details').forEach(detail => {
  detail.addEventListener('toggle', () => {
    if (!detail.open) return;
    document.querySelectorAll('.accordion details').forEach(other => {
      if (other !== detail) other.open = false;
    });
  });
});

document.querySelector('#year').textContent = new Date().getFullYear();
