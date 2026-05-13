FROM node:22-alpine AS builder

WORKDIR /src
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
ARG VITE_API_BASE_URL=/api
ARG VITE_AGENT_API_BASE_URL=/api/agent
ENV VITE_API_BASE_URL=${VITE_API_BASE_URL}
ENV VITE_AGENT_API_BASE_URL=${VITE_AGENT_API_BASE_URL}
RUN npm run build

FROM nginx:1.27-alpine

COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /src/dist /usr/share/nginx/html

EXPOSE 80
