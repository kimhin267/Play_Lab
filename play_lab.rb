require 'sinatra'

get '/' do
	return erb :index
end

get '/about' do
	return erb :about
end

get '/rates' do
	return erb :rates
end

get "/contact" do
	return erb :contact
end