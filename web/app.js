const showtimes = ["10:30 AM", "1:45 PM", "4:30 PM", "7:15 PM", "10:00 PM"];
const formatCurrency = (amount) => new Intl.NumberFormat("en-IN", {
  style: "currency",
  currency: "INR",
  maximumFractionDigits: 0
}).format(amount);
const takenSeats = new Set(["A3", "A4", "B6", "C2", "C3", "D7", "E1", "E8"]);
const movieGrid = document.querySelector("#movie-grid");
const searchInput = document.querySelector("#movie-search");
const bookingDialog = document.querySelector("#booking-dialog");
const confirmationDialog = document.querySelector("#confirmation-dialog");
const bookingForm = document.querySelector("#booking-form");
const dateInput = document.querySelector("#booking-date");
let movies = [];
let activeFilter = "All";
let selectedMovie = null;
let selectedShowtime = "";
const selectedSeats = new Set();

const today = new Date();
const localToday = new Date(today.getTime() - today.getTimezoneOffset() * 60000).toISOString().slice(0, 10);
dateInput.min = localToday;
dateInput.value = localToday;

function getGenreTag(movie) {
  if (/sci-fi/i.test(movie.genre)) return "Sci-Fi";
  if (/animation/i.test(movie.genre)) return "Animation";
  if (/comedy/i.test(movie.genre)) return "Comedy";
  return "Drama";
}

function renderMovies() {
  const query = searchInput.value.trim().toLowerCase();
  const visibleMovies = movies.filter((movie) => {
    const matchesGenre = activeFilter === "All" || getGenreTag(movie) === activeFilter;
    const matchesQuery = `${movie.title} ${movie.genre}`.toLowerCase().includes(query);
    return matchesGenre && matchesQuery;
  });
  if (!visibleMovies.length) {
    movieGrid.innerHTML = '<div class="empty-state">No films found. Try a different search.</div>';
    return;
  }
  movieGrid.innerHTML = visibleMovies.map((movie) => `
    <article class="movie-card">
      <div class="poster-art poster-${movie.poster}" role="img" aria-label="Stylized artwork for ${movie.title}">
        <span class="poster-symbol">${movie.title.split(/[\s:]/)[0].slice(0, 1)}</span>
        <div class="poster-label"><strong>${movie.title}</strong><span>NOW SHOWING</span></div>
      </div>
      <div class="movie-info">
        <div><h3>${movie.title}</h3><p>${movie.genre} <span aria-hidden="true">·</span> ${movie.duration}</p></div>
        <div class="movie-rating">★ ${movie.rating} <span>/ 10</span></div>
      </div>
      <div class="movie-actions"><span class="movie-duration">90+ MIN · THE BIG SCREEN</span><button class="book-button" data-movie-id="${movie.id}">Book tickets <span>↗</span></button></div>
    </article>`).join("");
}

async function loadMovies() {
  try {
    const response = await fetch("/api/movies");
    if (!response.ok) throw new Error(`Movie list request failed (${response.status}).`);
    movies = await response.json();
    renderMovies();
  } catch (error) {
    movieGrid.innerHTML = `<div class="empty-state">${error.message} Please refresh and try again.</div>`;
  }
}

function drawSeats() {
  const seatMap = document.querySelector("#seat-map");
  seatMap.innerHTML = "";
  ["A", "B", "C", "D", "E"].forEach((row) => {
    for (let number = 1; number <= 8; number += 1) {
      const seatId = `${row}${number}`;
      const seat = document.createElement("button");
      seat.type = "button";
      seat.className = `seat${takenSeats.has(seatId) ? " taken" : ""}${selectedSeats.has(seatId) ? " selected" : ""}`;
      seat.setAttribute("aria-label", `Seat ${seatId}${takenSeats.has(seatId) ? ", unavailable" : selectedSeats.has(seatId) ? ", selected" : ", available"}`);
      seat.setAttribute("aria-pressed", String(selectedSeats.has(seatId)));
      seat.disabled = takenSeats.has(seatId);
      seat.addEventListener("click", () => {
        if (selectedSeats.has(seatId)) selectedSeats.delete(seatId);
        else if (selectedSeats.size < 8) selectedSeats.add(seatId);
        drawSeats();
        updateTotal();
      });
      seatMap.append(seat);
    }
  });
}

function updateTotal() {
  const count = selectedSeats.size;
  document.querySelector("#seat-summary").textContent = count ? `${count} seat${count === 1 ? "" : "s"} · ${[...selectedSeats].sort().join(", ")}` : "Select your seats";
  document.querySelector("#total-price").textContent = formatCurrency(count * 14);
  document.querySelector("#confirm-booking").disabled = count === 0 || !selectedShowtime;
}

function openBooking(movie) {
  selectedMovie = movie;
  selectedShowtime = "";
  selectedSeats.clear();
  document.querySelector("#dialog-movie-title").textContent = movie.title;
  document.querySelector("#booking-poster").className = `booking-poster poster-${movie.poster}`;
  document.querySelector("#booking-meta").textContent = `${movie.genre} · ${movie.duration} · ★ ${movie.rating}`;
  document.querySelector("#showtime-list").innerHTML = "";
  showtimes.forEach((showtime) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "showtime-button";
    button.textContent = showtime;
    button.addEventListener("click", () => {
      selectedShowtime = showtime;
      document.querySelectorAll(".showtime-button").forEach((item) => item.classList.toggle("selected", item === button));
      updateTotal();
    });
    document.querySelector("#showtime-list").append(button);
  });
  document.querySelector("#form-message").textContent = "";
  drawSeats();
  updateTotal();
  bookingDialog.showModal();
}

document.querySelectorAll(".filter-chip").forEach((chip) => {
  chip.addEventListener("click", () => {
    activeFilter = chip.dataset.filter;
    document.querySelectorAll(".filter-chip").forEach((item) => item.classList.toggle("selected", item === chip));
    renderMovies();
  });
});

searchInput.addEventListener("input", renderMovies);
movieGrid.addEventListener("click", (event) => {
  const button = event.target.closest(".book-button");
  if (!button) return;
  const movie = movies.find((item) => String(item.id) === button.dataset.movieId);
  if (movie) openBooking(movie);
});

bookingForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!selectedMovie || !selectedShowtime || selectedSeats.size === 0) return;
  const submitButton = document.querySelector("#confirm-booking");
  const message = document.querySelector("#form-message");
  submitButton.disabled = true;
  submitButton.firstChild.textContent = "Confirming...";
  message.textContent = "";
  const form = new FormData(bookingForm);
  const payload = {
    movieId: String(selectedMovie.id),
    date: form.get("date"),
    showtime: selectedShowtime,
    seats: [...selectedSeats].sort(),
    name: form.get("name").trim(),
    email: form.get("email").trim()
  };
  try {
    const response = await fetch("/api/bookings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || "We couldn't complete your booking.");
    bookingDialog.close();
    document.querySelector("#confirmation-copy").textContent =
      `${result.movie} · ${result.date} · ${result.showtime} · ${result.seats.join(", ")} · ${formatCurrency(result.total)}`;
    document.querySelector("#confirmation-code").textContent = `BOOKING ${result.bookingId}`;
    confirmationDialog.showModal();
  } catch (error) {
    message.textContent = error.message;
  } finally {
    submitButton.disabled = selectedSeats.size === 0 || !selectedShowtime;
    submitButton.firstChild.textContent = "Confirm booking ";
  }
});

document.querySelector("#dialog-close").addEventListener("click", () => bookingDialog.close());
document.querySelector("#confirmation-close").addEventListener("click", () => confirmationDialog.close());
document.querySelector("#done-button").addEventListener("click", () => confirmationDialog.close());
loadMovies();
