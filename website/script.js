const header = document.querySelector('.site-header');
const menu = document.querySelector('.menu-button');
const links = document.querySelector('.nav-links');

const updateHeader = () => header?.classList.toggle('scrolled', window.scrollY > 12);

const closeMenu = () => {
  menu?.classList.remove('open');
  links?.classList.remove('open');
  menu?.setAttribute('aria-expanded', 'false');
};

menu?.addEventListener('click', () => {
  const open = menu.getAttribute('aria-expanded') !== 'true';
  menu.classList.toggle('open', open);
  links?.classList.toggle('open', open);
  menu.setAttribute('aria-expanded', String(open));
});

links?.querySelectorAll('a').forEach(link => link.addEventListener('click', closeMenu));
window.addEventListener('scroll', updateHeader, { passive: true });
window.addEventListener('resize', () => {
  if (window.innerWidth > 700) closeMenu();
});

updateHeader();
document.querySelector('#year').textContent = new Date().getFullYear();
